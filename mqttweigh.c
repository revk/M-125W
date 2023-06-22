// MQTT Weight logger

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdlib.h>
#include <ctype.h>
#include <err.h>
#include <sqllib.h>
#include <mosquitto.h>

int debug = 0;

#define Q(x) #x                 // Trick to quote defined fields
#define QUOTE(x) Q(x)

int
main (int argc, const char *argv[])
{
#ifdef  SQLCONFIG
   const char *sqlconfig = QUOTE (SQLCONFIG);
#else
   const char *sqlconfig = NULL;
#endif
#ifdef  SQLHOST
   const char *sqlhost = QUOTE (SQLHOST);
#else
   const char *sqlhost = NULL;
#endif
#ifdef  SQLUSER
   const char *sqluser = QUOTE (SQLUSER);
#else
   const char *sqluser = NULL;
#endif
#ifdef  SQLPASS
   const char *sqlpass = QUOTE (SQLPASS);
#else
   const char *sqlpass = NULL;
#endif
#ifdef  SQLDATABASE
   const char *sqldb = QUOTE (SQLDATABASE);
#else
   const char *sqldb = "weigh";
#endif
#ifdef  SQLTABLE
   const char *sqltable = QUOTE (SQLTABLE);
#else
   const char *sqltable = "weight";
#endif
#ifdef  MQTTHOST
   const char *mqtthost = QUOTE (MQTTHOST);
#else
   const char *mqtthost = "localhost";
#endif
#ifdef  MQTTUSER
   const char *mqttuser = QUOTE (MQTTUSER);
#else
   const char *mqttuser = NULL;
#endif
#ifdef  MQTTPASS
   const char *mqttpass = QUOTE (MQTTPASS);
#else
   const char *mqttpass = NULL;
#endif
#ifdef  MQTTID
   const char *mqttid = QUOTE (MQTTID);
#else
   const char *mqttid = NULL;
#endif
#ifdef  MQTTTOPIC
   const char *mqtttopic = QUOTE (MQTTTOPIC);
#else
   const char *mqtttopic = "stat/+/weight";     // From M-125W code in Marsden
#endif
#ifdef  PREFIX
   const char *prefix = QUOTE (PREFIX);
#else
   const char *prefix = NULL;
#endif
   poptContext optCon;          // context for parsing command-line options
   {                            // POPT
      const struct poptOption optionsTable[] = {
	      // *INDENT-OFF*
         {"mqtt-host", 'h', POPT_ARG_STRING | (mqtthost ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqtthost, 0, "MQTT hostname", "Hostname"},
         {"mqtt-user", 'u', POPT_ARG_STRING | (mqttuser ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqttuser, 0, "MQTT username", "Username"},
         {"mqtt-pass", 'P', POPT_ARG_STRING | (mqttpass ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqttpass, 0, "MQTT password", "Password"},
         {"mqtt-id", 0, POPT_ARG_STRING | (mqttid ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqttid, 0, "MQTT ID", "ID"},
         {"mqtt-topic", 't', POPT_ARG_STRING | (mqtttopic ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqtttopic, 0, "MQTT topic", "Topic"},
         {"prefix", 0, POPT_ARG_STRING | (prefix ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &prefix, 0, "Weight prefix", "text"},
         {"sql-config", 'c', POPT_ARG_STRING | (sqlconfig ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqlconfig, 0, "MQTT config", "Filename"},
         {"sql-host", 's', POPT_ARG_STRING | (sqlhost ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqlhost, 0, "MQTT hostname", "Hostname"},
         {"sql-user", 0, POPT_ARG_STRING | (sqluser ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqluser, 0, "MQTT username", "Username"},
         {"sql-pass", 0, POPT_ARG_STRING | (sqlpass ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqlpass, 0, "MQTT password", "Password"},
         {"sql-database", 'd', POPT_ARG_STRING | (sqldb ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqldb, 0, "MQTT database", "Database"},
         {"sql-table", 't', POPT_ARG_STRING | (sqltable ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqltable, 0, "MQTT table", "Table"},
         {"sql-debug", 'v', POPT_ARG_NONE, &sqldebug, 0, "SQL Debug"},
         {"debug", 'V', POPT_ARG_NONE, &debug, 0, "Debug"},
         POPT_AUTOHELP {}
	      // *INDENT-ON*
      };

      optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
      //poptSetOtherOptionHelp (optCon, "");

      int c;
      if ((c = poptGetNextOpt (optCon)) < -1)
         errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

      if (poptPeekArg (optCon))
      {
         poptPrintUsage (optCon, stderr, 0);
         return -1;
      }
   }

   SQL sql;
   sql_real_connect (&sql, sqlhost, sqluser, sqlpass, sqldb, 0, NULL, 0, 1, sqlconfig);

   void message (struct mosquitto *mqtt, void *obj, const struct mosquitto_message *msg)
   {
      obj = obj;
      char *topic = msg->topic;
      char *val = malloc (msg->payloadlen + 1);
      if (!val)
         errx (1, "malloc");
      if (msg->payloadlen)
         memcpy (val, msg->payload, msg->payloadlen);
      val[msg->payloadlen] = 0;
      char *v = val;
      if (debug)
         warnx ("Message %s %s", topic, val);
      if (!strncmp (v, "{\"SerialReceived\":\"", 19))
         v += 19;               // Allow for use with Tasmota serial bridge
      if (!prefix || !strncmp (v, prefix, strlen (prefix)))
      {
         if (prefix)
            v += strlen (prefix);
         while (*v == ' ')
            v++;
         char *e = v;
         while (isdigit (*e) || *e == '.')
            e++;
         while (*e == ' ')
            *e++ = 0;
         double kg = 0;
         if (!strncmp (e, "st", 2))
         {                      // FFS
            *e = 0;
            e += 2;
            double st = strtod (v, NULL);
            while (*e == ' ')
               e++;
            v = e;
            while (isdigit (*e) || *e == '.')
               e++;
            while (*e == ' ')
               *e++ = 0;
            if (!strncmp (e, "lb", 1))
            {
               *e = 0;
               double lb = strtod (v, NULL);
               lb += st * 14;
               kg = lb / 2.2;
            }
         } else if (!strncmp (e, "lb", 2))
         {                      // Arrg
            *e = 0;
            double lb = strtod (v, NULL);
            kg = lb / 2.2;
         } else
         {                      // Assume kg
            *e = 0;
            kg = strtod (v, NULL);
         }
         if (kg)
         {
            const char *a = mqtttopic;
            char *b = topic;
            while (*a && *a == *b && *a != '#' && *a != '+')
            {
               a++;
               b++;
            }
            if (*a == '+')
            {
               b = strdupa (b);
               char *e = b;
               while (*e && *e != '/')
                  e++;
               *e = 0;
            } else if (*a != '#')
               b = topic;
            sql_safe_query_free (&sql, sql_printf ("INSERT INTO `%S` SET `Topic`=%#s,kg=%.1lf", sqltable, b, kg));
         }
      }
      free (val);
   }

   int e = mosquitto_lib_init ();
   if (e)
      errx (1, "MQTT init failed %s", mosquitto_strerror (e));
   struct mosquitto *mqtt = mosquitto_new (mqttid, 1, NULL);
   e = mosquitto_username_pw_set (mqtt, mqttuser, mqttpass);
   if (e)
      errx (1, "MQTT auth failed %s", mosquitto_strerror (e));
   mosquitto_message_callback_set (mqtt, message);
   e = mosquitto_connect (mqtt, mqtthost, 1883, 60);
   if (e)
      errx (1, "MQTT connect failed (%s) %s", mqtthost, mosquitto_strerror (e));
   e = mosquitto_subscribe (mqtt, NULL, mqtttopic, 0);
   if (e)
      errx (1, "MQTT subscribe failed %s", mosquitto_strerror (e));
   e = mosquitto_loop_forever (mqtt, 1000, 1);
   if (e)
      errx (1, "MQTT loop failed %s", mosquitto_strerror (e));
   mosquitto_destroy (mqtt);
   mosquitto_lib_cleanup ();

   sql_close (&sql);
   poptFreeContext (optCon);
   return 0;
}
