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
   const char *sqldb = "weight";
#endif
#ifdef  SQLTABLE
   const char *sqltable = QUOTE (SQLTABLE);
#else
   const char *sqltable = "weight";
#endif
#ifdef  MQTTHOST
   const char *mqtthost = QUOTE (MQTTHOST);
#else
   const char *mqtthost = NULL;
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
   const char *mqtttopic = "tele/scales/RESULT";        // Tasmota Serial received logic
#endif
   {                            // POPT
      poptContext optCon;       // context for parsing command-line options
      const struct poptOption optionsTable[] = {
	      // *INDENT-OFF*
         {"mqtt-host", 0, POPT_ARG_STRING | (mqtthost ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqtthost, 0, "MQTT hostname", "Hostname"},
         {"mqtt-user", 0, POPT_ARG_STRING | (mqttuser ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqttuser, 0, "MQTT username", "Username"},
         {"mqtt-pass", 0, POPT_ARG_STRING | (mqttpass ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqttpass, 0, "MQTT password", "Password"},
         {"mqtt-id", 0, POPT_ARG_STRING | (mqttid ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqttid, 0, "MQTT ID", "ID"},
         {"mqtt-topic", 0, POPT_ARG_STRING | (mqtttopic ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &mqtttopic, 0, "MQTT topic", "Topic"},
         {"sql-config", 0, POPT_ARG_STRING | (sqlconfig ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqlconfig, 0, "MQTT config", "Filename"},
         {"sql-host", 0, POPT_ARG_STRING | (sqlhost ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqlhost, 0, "MQTT hostname", "Hostname"},
         {"sql-user", 0, POPT_ARG_STRING | (sqluser ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqluser, 0, "MQTT username", "Username"},
         {"sql-pass", 0, POPT_ARG_STRING | (sqlpass ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqlpass, 0, "MQTT password", "Password"},
         {"sql-database", 0, POPT_ARG_STRING | (sqldb ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqldb, 0, "MQTT database", "Database"},
         {"sql-table", 0, POPT_ARG_STRING | (sqltable ? POPT_ARGFLAG_SHOW_DEFAULT : 0), &sqltable, 0, "MQTT table", "Table"},
         {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
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
      poptFreeContext (optCon);
   }

   SQL *sql = sql_real_connect (NULL, sqlhost, sqluser, sqlpass, sqldb, 0, NULL, 0, 1, sqlconfig);

   int e = mosquitto_lib_init ();
   if (e)
      errx (1, "MQTT init failed %s", mosquitto_strerror (e));

   sql_close (sql);
   return 0;
}
