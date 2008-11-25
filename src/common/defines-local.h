/* Purpose: Define server type. Included exclusively by "defines.h". */


/* If the server is a normal TomeNET server, simply leave all of the
   special server rules below commented out. - C. Blue */


/* Server is a true RPG server? - C. Blue - This means..
 * - 1 real-life person may only create up to 1 character in total
 * - all characters are no-ghost by default (death is permanent)
 * - dungeons are IRONMAN
 * - and many more things that are just a little bit different :)
 */
/* #define RPG_SERVER */

/* Server is Moltor's Smash Arcade server?
   (allows additional definition of RPG_SERVER if desired!) */
/* #define ARCADE_SERVER */

/* Server running in 'Fun Mode'? Allows cheezing and cheating and everything. */
/* #define FUN_SERVER */
