/*
   FILE NAME:   ticker.c

   TITLE:       Package for work with ticker

   DESCRIPTION: This simple package slightly makes easy evaluation and
       output of work duration of program components.

*/

#ifdef HAVE_CONFIG_H
#include "cocom-config.h"
#else /* In this case we are oriented to ANSI C */
#endif /* #ifdef HAVE_CONFIG_H */

#include <stdio.h>
#include "ticker.h"

/* We don't use times or getrusage here because they have the same
   accuracy as clock on major machines Linux, Solaris, AIX.  The
   single difference is IRIX on which getrusage is more accurate. */

/* The following macro is necessary for non standard include files of
   SUNOS 4..., linux */

#ifndef CLOCKS_PER_SECOND
#ifdef CLOCKS_PER_SEC
#define CLOCKS_PER_SECOND CLOCKS_PER_SEC
#elif __linux__
#define CLOCKS_PER_SECOND 100
#elif sun
#define CLOCKS_PER_SECOND 1000000
#elif CLK_TCK
#define CLOCKS_PER_SECOND CLK_TCK
#else
#error define macro CLOCKS_PER_SECOND
#endif
#endif /* CLOCKS_PER_SECOND */

/* The following function creates ticker and makes it active. */

ticker_t
create_ticker (void)
{
  ticker_t ticker;

  ticker.modified_creation_time = clock ();
  ticker.incremented_off_time = 0;
  return ticker;
}

/* The following function switches off given ticker. */

void
ticker_off (ticker_t *ticker)
{
  if (ticker->incremented_off_time == 0)
    ticker->incremented_off_time = clock () + 1;
}

/* The following function switches on given ticker. */

void
ticker_on (ticker_t *ticker)
{
  if (ticker->incremented_off_time != 0)
    {
      ticker->modified_creation_time
        += clock () - ticker->incremented_off_time + 1;
      ticker->incremented_off_time = 0;
    }
}

/* The following function returns current time since the moment when
   given ticker was created.  The result is measured in seconds as
   float number. */

double
active_time (ticker_t ticker)
{
  if (ticker.incremented_off_time != 0)
    return (((double) (ticker.incremented_off_time - 1
                       - ticker.modified_creation_time))
            / CLOCKS_PER_SECOND);
  else
    return (((double) (clock () - ticker.modified_creation_time))
            / CLOCKS_PER_SECOND);
}

/* The following function returns string representation of active time
   of given ticker.  The result is string representation of seconds with
   accuracy of 1/100 second.  Only result of the last call of the
   function exists.  Therefore the following code is not correct

      printf ("parser time: %s\ngeneration time: %s\n",
              active_time_string (parser_ticker),
              active_time_string (generation_ticker));

   Correct code has to be the following

      printf ("parser time: %s\n", active_time_string (parser_ticker));
      printf ("generation time: %s\n",
              active_time_string (generation_ticker));

*/

const char *
active_time_string (ticker_t ticker)
{
  static char str [40];

  sprintf (str, "%.2f", active_time (ticker));
  return str;
}
