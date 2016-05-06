/* Based on /obj/coins */

#include <const.h>
#include <saveload.h>
#include <desc.h>
#include <object.h>
#include <item.h>
#include <quantity.h>
#include <stackobj.c>

NAME( "quarter" )
ALT_NAME( ({ "coin" }) )
DISTANT( "a quarter" )
SPECIFIC( "the quarter" )
LOOK( "A large cupronickel coin with George Washington stamped on it.  A quarter is worth 25 cents." )
PLURAL( "pennies" )
ALT_PLURAL( ({ "coins" }) )
TYPE( "coin" )
GETTABLE( 1 )
DROPPABLE( 1 )
WEIGHT( 6 )
BULK( 5 )
VALUE( 25 )      // 1p = 1p ;)

ICON( '$', 0x8B, LAYER_ITEMS )

void create() {
   ::create();
   set_quantity( 1 );
}

/// For lack of a better function name
string query_issuing_government() {
   return "united states";
}

/// Indicates that this object is usable as money.
int query_is_money() {
   return 1;
}
