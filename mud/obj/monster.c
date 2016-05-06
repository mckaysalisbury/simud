#include <living.h>
#include <inven.h>
#include <object.h>
#include <ai.h>

/** Will contain code for handling monsters. For now just a bare-bones
 * living object with minor extensions.
 */

object possessor;

// for butchering
int product_aspect;
mapping product_map;

void load_self( int id ) {
   ::load_self( id );
   set_living_name( name );
}

void create() {
   ::create();
   product_map = ([]);
   set_readiness(MAX_READINESS);
   add_action("search_bin", "", 1);
   set_living_name( query_name() );
}

int query_monster( ) {
   return 1;
}

string locate_command( string file ) {
   if( file_exists("/bin/"+file+".c") ) {
      return "/bin/" + file;
   } else if( file_exists("/bin/monster/"+file+".c") ) {
      return "/bin/monster/" + file;
   } else if( file_exists("/bin/soul/"+file+".c") ) {
      return "/bin/soul/" + file;
   } else {
      return 0;
   }
}

// on_skin is called when you attempt to skin a corpse. Returns an object
// var of the skin.
/*
 * deprecated by new butchering system (8/12/04)
object on_skin() {
   string str;
   object ob;

   str = query_env_var( "skin_type" );

   if( str == "skinless" || str == 0 ) return 0;

   ob = clone_object( query_env_var("skin_type") );
   if( !ob ) {
      debug( "Error in monster.c: skin_type not valid in on_skin()" );
      return 0;
   }
   set_env_var( "skin_type", "skinless" );
   return ob;
}
*/

int query_product_aspect() { return product_aspect; }
void set_product_aspect(int x) { product_aspect = x; }
int *query_product_list() { return m_indices(product_map); }
int query_product_quantity() {
   int x = 0, item;
   foreach ( item : m_indices(product_map) ) {
      if (pointerp(product_map[item]))
         x += product_map[item][1];
   }
   return x;
}
mapping query_product_map() { return product_map; }
void set_product_map(mapping m) { product_map = m; }
void add_product(int aspect, int amt, int dc) {
   product_map[aspect] = ({ amt, dc });
}

// changed 8/12/04 by al to the new and beefier butchering system
object on_butcher() {
   object ob;
   int dc, item, *products;

   // make sure it's still legal to butcher me
   if ( !query_product_aspect() || !query_product_quantity() )
      return 0;

   // select a random product for butchering
   products = query_product_list();
   item = products[random(sizeof(products))];

   // actually create the item now
   switch (item) {
      case C_MEAT: ob = clone_object("/items/food/meat"); break;
      case C_PELT: ob = clone_object("/econ/animal/skin"); break;
      case C_CARAPACE: ob = clone_object("/econ/animal/carapace"); break;
      case C_FEATHER: ob = clone_object("/econ/animal/feather"); break;
      case C_SCALE: ob = clone_object("/econ/animal/scale"); break;
      //case C_BONE: ob = clone_object("/econ/animal/bone"); break;
      //case C_SKULL: ob = clone_object("/econ/animal/skull"); break;
      //case C_HORN: ob = clone_object("/econ/animal/horn"); break;
      //case C_GREASE: ob = clone_object("/econ/animal/grease"); break;
      //case C_TOOTH: ob = clone_object("/econ/animal/tooth"); break;
      //case C_CLAW: ob = clone_object("/econ/animal/claw"); break;
      //case C_TAIL: ob = clone_object("/econ/animal/tail"); break;
      //case C_WING: ob = clone_object("/econ/animal/wing"); break;
      //case C_FOOT: ob = clone_object("/econ/animal/foot"); break;
      //case C_STINGER: ob = clone_object("/econ/animal/stinger"); break;
      default: debug("crafting error, attempt to make "+item);
               return 0;
   }
   ob->set_product_aspect( query_product_aspect() );
   dc = product_map[item][1];

   // special case for skins
   if (item == C_PELT || item == C_CARAPACE) {
      ob->set_size(product_map[item][0]);
      product_map -= ([item]);
   } else {
      // deduct the item from the list
      product_map[item][0]--;
      if (product_map[item][0] == 0)
         product_map -= ([item]);
   }

   // make the skill check
   set_actor(this_player());
   set_target(ob);
   if (!this_player()->skill_check("craft.butcher",dc)) {
      // rest of special case for skins
      if (item == C_PELT || item == C_CARAPACE) {
         // failed skill check results in damaged skin
         ob->set_size(max(1,random(ob->query_size()-1)));
         if (ob->query_size() < 1) {
            this_player()->msg_local("~CWRN~Name ~verbdamage part of ~targ.~CDEF");
         } else {
            this_player()->msg_local("~CWRN~Name ~verbfail to ~verbcarve ~targ from the corpse.~CDEF");
            destruct(ob);
            return 0;
         }
      } else {
        // everything else is pass/fail
        this_player()->msg_local("~CWRN~Name ~verbfail to ~verbcarve ~targ from the corpse.~CDEF");
        destruct(ob);
        return 0;
      }
   }

   return ob;
}

int search_bin(string params) {
   string command;
   string verb_obj;
   string a, b;

   command = query_verb();
   if( sscanf(command, "%s.%s", a, b) == 2 || sscanf(command, "%s/%s", a, b) == 2 ) return 0;

   if (0 != (verb_obj = locate_command(command))) {
       return call_other(verb_obj, "main", params);
   }
   return 0;
}

int query_allow_possess() {
   return 1;
}

void set_possessor( object who ) {
   possessor = who;
}

object query_possessor() {
   return possessor;
}

void catch_tell( string text ) {
   if( possessor ) {
      // Messages to my possessor are tagged, to distinguish them.
      tell_object( possessor, "\255" + text );
   }
}

int query_ansi() {
   if( possessor )
      return possessor->query_ansi();
   return 0;
}

int query_term_width() {
   if( possessor )
      return possessor->query_term_width();
   return 0;
}

int query_term_height() {
   if( possessor )
      return possessor->query_term_height();
   return 0;
}

void on_hunger() {
   if( query_env_var("immortal") || query_env_var("nohunger") )
      set_food( query_food_capacity() * 3 / 4 );
   else ::on_hunger();
}

void on_thirst() {
   if( query_env_var("immortal") || query_env_var("nothirst") )
      set_drink( query_drink_capacity() * 3 / 4 );
   else ::on_thirst();
}

int range(int top) {
   int bottom = max(top / 4, 1);
   return min(bottom + random(top - bottom), top);
}

void set_random_stats() {
   object race = this_object()->query_race_object();
   string stat, *stats = ({ "str", "dex", "con", "spd", "wil", "int", "emp", "cha" });
   foreach( stat : stats )
      this_object()->set_stat(stat, range(race->query_stat_max(stat)) );
}
