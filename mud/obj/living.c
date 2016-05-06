#include <const.h>
#include <emote.h>
#include <desc.h>
#include <const/battconst.h>
#include <item.h>
#include <stats.h>
#include <const/exitsconst.h>
#include <family.h>
#include <genetics.h>
#include <faction.h>
#include <magic.h>

#define SK_DAEMON "/daemon/skill"
#define FAITH_DAEMON "/faith/prayer_daemon"
#ifndef PARTY_DAEMON
#define PARTY_DAEMON "/daemon/party"
#endif

/** This object is inherited by all living objects. */
/// Properties and behaviors possessed by all living objects.

// Properties and handling functions of a living object.
// All living objects possess these stats.

/// Level of this object -- Describes its rank.
int level;
/// What specialized rank does this object have?
string job_title;
/// 0 = neuter, 1 = male, 2 = female
int gender;
/// A three-element array containing information on good/evil, lawful/chaotic & custom alignment.
mixed *alignment;
/// The total number of times this living has died since we started keeping track.
int deaths;
/// The current count to heal_delay.
nosave int heal_count;
/// The current count to mana_heal_delay.
nosave int mana_heal_count;
/// The current count to end_heal_delay.
nosave int end_heal_count;
/// Counter to limit action repetition per heart beat.
nosave int actions_this_beat;
/// Combat readiness
int readiness;
/** A list of people/things whose names I know. This determines how they
 * are described to me.
 */
mapping names_known;
int ac; ///< Base natural armour class
mapping ac_mod, //< Specific damage types, delta values from base ac
        wc; ///< Weapon class
int hp; ///< How much more damage can be taken before I die
int damage_pending; ///< How much damage they are going to take at the end of this hb
//    maxhp; // Maximum value of hp, determined by constitution.
int mana; ///< How much inherent mana am I channeling
int endurance; ///< How much strenuous activity can I engage in before I become utterly exhausted

/// Units of food the living has eaten
int food;
/// Units of drink the living has drunk
int drink;

/// A mapping of this living object's skills. The values are the level of the skills.
mapping skills;
/// A list of all players (and npc's) that this player trusts.
mapping trust;
/// A mapping of how well the gods like this living
mapping favor;

/// Time stamps for when skill checks were made. Allows a skill check to be 'sticky'.
nosave mapping cached_skill_time = ([ ]);
/// Result stamps from when skill checks were made. Allows a skill check to be 'sticky'.
nosave mapping cached_skill_value = ([ ]);

// Fighting info
nosave object *target;      ///< An array of targets. target[0] is the current target.
nosave mixed *action;       ///< Queue of commands to perform in combat. action[0] is next.
string curr_form;           ///< The form you are currently fighting with; indexes into known_forms
/** A 3d mapping; key is the name of the form, the first value is your level, the second
 * is an array of attitude allocations, and the third value is your experience with it.
 */
mapping known_forms;
mapping known_spells;
string *known_recipes;   ///< What crafting recipes does this living know?

/// True if this player is in 'combat' mode.
int combat;

/// The last neuter, male, and female things you looked at, respectively.
nosave mixed *pronoun;

/// A flag for whether this player is dead.
int dead;
/// True if the dead player is a corpse.
nosave int is_corpse;
/// A flag for whether the object is the victim of some hold effect.
int held;
/// A flag for whether the object is performing some long action.
int busy;
/// Percentage of hp to run away at.
int wimpy;
/// True if this living is attempting to soak damage.
int soak;
/// Current stance (standing, seated, etc...)
int stance;
/// This living object's race, human, dwarf, canine, etc...
string race;
/// Allows a monster or player to have a default thing to be doing when they're doing nothing.
string default_doing;
/// An extended "distant" description which describes what the player is currently doing.
nosave string doing;
/// Used to evaluate ~targ codes in doing strings.
nosave object doing_target;
/// A count-down. When it hits zero, the "doing" action resets to null.
nosave int doing_delay;

/// Object array to store who is following you.
nosave object *my_followers;
nosave object *my_leaders;

/// The player's environment variables
mapping env_vars;

/// Initialization code for a living object.
void _create_living() {
   ac = 0;
   ac_mod = ([]);
   wc = ([]);
   alignment = allocate(3);
   skills = m_allocate(10, 4);
   // Baseline HP
   if ( this_object()->query_max_hp() )
      hp = this_object()->query_max_hp();
   else
      hp = 75;
   set_weight( 68000 );
   // Baseline stats
   stats = (["con":10, "str":10, "dex":10, "spd":10, "wil":10, "cha":10,
             "int":10, "emp":10]);
   stat_use = (["con":1000, "str":1000, "dex":1000, "spd":1000, "wil":1000,
                "cha":1000, "int":1000, "emp":1000]);
   stat_bonus = ([]);

   enable_commands();
   // By default, enable heart beat. It'll turn itself
   // off if it needs to. (Except for blueprints, which should remain
   // idle -- Acius)
   if( is_clone(this_object()) )
     set_heart_beat( 1 );
   stance = ST_STANDING;
   known_forms = ([ "brawl": 0 ]);
   known_spells = ([]);
   known_recipes = ({});
   curr_form = "brawl";
   pronoun = ({ ({ }), ({ }), ({ }), ({ }) });

   call_out("validate_skills",10);
}

/// Used by the object editor to let you edit living objects
string living_info() {
   return "level\ngender\nweight\nspeed\nconstitution\nstrength\ndexterity\nwill\ncharm\nintelligence\nempathy\nhp\nmaxhp";
}

/************************************************************* PROTOTYPES */

int query_skill(string skill);
varargs int skill_check(string skill, varargs int *dc);
void drop_invalid_targets();
varargs void msg_local(string words, int indent);
void recalc();
void set_skill(string str, int x);
object die();
varargs object query_form_object( string form );
int query_real_ac();
object query_race_object();
int remove_follower( object x );
int remove_leader( object x );
int practice_skill_conditional( string skill, int cutoff, int amt );
varargs void practice_skill( string skill, int amt );
void set_actions( mixed * x );
varargs void say_all(string words, int indent);
void set_movement_type( string str );

/**************************************************************** QUERIES */

/// Todo: Nuke this once it's no longer used.
int query_living() { DEP("query_living", "query_is_living"); return !dead; }

/** Returns true. Denotes that this object is living.
 * Naturally other objects can forge this if they desire.
 */
int query_is_living() { return !dead; }

/// \return The level/rank of this object.
int query_level() { return level; }

/// \return The number of times this living has died since we started keeping count
int query_deaths() { return deaths; }

mixed * query_pronoun() {
   return pronoun;
}

/// \return The actual 'level' of this object.
int calculate_level(int total_skills) {
  // old math was evil and O(n)... so I killed it and am using stupid
  // simple method in stead - Al (sept.6.03)
  return total_skills / 25;
}

/// \return The total number of skill levels this living has.
int calculate_skill_total() {
  int total = 0;
  string str;
  foreach (str : m_indices(skills))
    total += this_object()->query_skill_level(str);
  return total;
}

/// \return The job title of this object.
string query_job_title() { return job_title; }

/// \return The current 'combat' stance of the object.
int query_stance() { return stance; }

/// \return One of three possible genders -- 0 = neuter, 1 = male, 2 = female.
int query_gender() { return gender; }

/// \return This object's party object, if any.
object query_party_object() { return present_clone("/obj/party"); }

/// \return This object's current degree of readiness (in combat)
int query_readiness() { return readiness; }

/// \return The readiness requirement for the next action in the queue
int readiness_required() {
  // if the queue is empty, assume MAX_READINESS
  if( !pointerp(action) || sizeof(action) < 1 )
    return MAX_READINESS;
  return (action[0][0]);
}

/// \return This object's bonus to wc from current strength.
int query_str_wc() {
   switch (query_strength()) {
      case 1..3: return -3; break;
      case 4..6: return -2; break;
      case 7..8: return -1; break;
      case 9..11: return 0; break;
      case 12..17: return (query_strength()-10)/2; break;
      case 18..20: return 4; break;
      default: return (query_strength()-16);
   }
   return 0; // never reached, is to quiet 'missing return' warnings
}

/// \return How many hit points this object has.
int query_hp() { return hp; }

/// \return True, if this player has been introduced to <name> before; false otherwise.
int query_recognize( string name ) {
   if( !names_known ) return 0;
   return name == this_object()->query_name() || member(names_known, name);
}

/// \return A mapping of people that this player recognizes.
mapping query_names_known() { return names_known; }


/// \return How much mana this object has.
int query_mana() { return mana; }

/// \return How much endurance this object has.
int query_endurance() { return endurance; }

/// \return This object's current food level.
int query_food() { return food; }

/// \return This object's current drink level.
int query_drink() { return drink; }

/// \return True if this living will automatically take items given to it.
int query_autotake() { return this_object()->query_env_var("Tautotake"); }

/// \return This object's current autoeat setting.
string query_autoeat() { return this_object()->query_env_var("Tautoeat"); }

/// \return This object's current autodrink setting.
string query_autodrink() { return this_object()->query_env_var("Tautodrink"); }

/// \return This object's maximum food capacity.
int query_food_capacity() {
   int cap;
   cap = ((query_weight()-68000)/2000) + (72*query_real_constitution());
   if( cap < 50 ) return 50;
   return cap;
}

/// \return This object's maximum drink capacity.
int query_drink_capacity() {
   int cap;
   cap = ((query_weight()-68000)/2000) + (48*query_real_constitution());
   if( cap < 10 ) return 10;
   return cap;
}

/// \return The maximum weight, in grams, that this living object can carry.
int query_weight_capacity() {
   int cap;
   cap = query_str() * 10000;
   return cap;
}

/// \return This object's food percentage
int query_food_percent() { return (100 * query_food() / query_food_capacity()); }

/// \return This object's drink percentage
int query_drink_percent() { return (100 * query_drink() / query_drink_capacity()); }

/// \return This object's weapon class.
varargs mixed query_wc(string x) {
  if (x && stringp(x))
    return wc[x];
  else if (!x)
    return wc;
  else
    return 0;
}

/** This function returns information about the armour a living object happens
 * to be wearing at the moment. If a parameter is given, the returned value is
 * the total ac offered by armour of a given type.
 *
 * If no parameter is given, the function returns an array containing the
 * total base ac of all armour in the first element and an ac_mod style
 * mapping in the second element.
 */
varargs mixed query_armour_ac(string str) {
  object item, *armour = BATTLE_DAEMON->query_worn_armour(this_object());
  int x;
  mapping temp;

  debug("query_armour_ac called (po is "+as_string(previous_object())+"). This function ignores body parts, and isn't useful.", "combat");
  if (!armour) {
    if (str)
      return 0;
    else
      return ({ 0, ([]) });
  } if (str && stringp(str)) {
    foreach (item: armour)
      x += item->query_ac(str);
    return x;
  } else {
    foreach (item: armour) {
      x += item->query_base_ac();
      temp = item->query_ac_mod();
      if( !mappingp(temp) ) temp = ([ ]);
    }
    return ({ x, temp });
  }
}

/** \return This object's armour class.
 * \warn Does not return the variable ac. To get the value of ac, use query_real_ac().
 */
varargs mixed query_ac(string x) {
  mapping temp;
  string type;
  int value;
  mixed armour;

  debug( "query_ac() called from " + as_string(previous_object()) + "; this probably is wrong, since the return values aren't meaningful.", "combat" );
  // specific ac type
  if( stringp(x) )
    return ( query_real_ac() + ac_mod[x] + query_armour_ac(x) );
  else if (x)   // they tried to ask for ac[5] or something stupid...
    return 0;

  // no specific type requested, return everything
  temp = copy(ac_mod);
  armour = query_armour_ac();
  if (mappingp(armour[1]))
    temp += armour[1];
  foreach ( type, value : ac_mod )
    temp[type] += query_real_ac() + armour[0];
  return temp;
}

/// \return This object's ac modifier.
varargs mixed query_ac_mod(string x) {
  if (x && stringp(x))
    return ac_mod[x];
  else if (!x)
    return ac_mod;
  else
    return 0;
}

/// \return This object's base ac (without armour).
int query_real_ac() {
  if (!intp(ac))
    ac = 0;
  return ac;
}

/// \return This object's general ac (most armour included).
int query_average_ac() {
  float total = (float)query_real_ac(), tmp;
  object obj;
  foreach ( obj : BATTLE_DAEMON->query_worn_armour(this_object()) ) {
    int bp = obj->query_body_parts(), ac = obj->query_base_ac();
    tmp = 0;
    if (bp & BP_HEAD)
      tmp += (ac * 0.2);
    if (bp & BP_LEGS)
      tmp += (ac * 0.3);
    if (bp & BP_FEET)
      tmp += (ac * 0.1);
    if (bp & BP_ARMS)
      tmp += (ac * 0.4);
    if (bp & BP_BACK)
      tmp += (ac * 0.6);
    if (bp & BP_CHEST)
      tmp += (ac * 0.8);
    //debug(obj->query_name()+", ac = "+ac+", tmp = "+tmp, "combat");
    total += tmp;
  }
  return (int)total;
}

/// \return True if this living object is dead, false if it is not. The value of dead will be how far it has decayed.
int query_dead() { return dead; }

/// \return Number of hb remaining on hold effect, -1 is perpetual.
int query_held() { return held; }

/// \return Number of hb remaining of business, -1 is perpetual.
int query_busy() { return busy; }

/// \return Percentage of hp to wimpy at.
int query_wimpy() { return wimpy; }

/// \return True if this living is attempting to soak damage into endurance.
int query_soak() { return soak; }

/// \return Default movement type for this living's race
string query_default_movement_type() {
   string str = query_race_object()->query_default_movement_type();
   if (!str)
      return "walk";
   else
      return str;
}

/// \return Verb to describe movement around/between rooms
string query_movement_type() {
   string str = this_object()->query_env_var("movement_type");
   if (!str) {
      str = query_default_movement_type();
     set_movement_type(str);
   }
   return str;
}

/// \return The amount of time, in heart beats, for a single hit point to heal.
int query_heal_delay() {
   return max(45 - query_stat("con")
                 - query_stat("wil")
                 - query_skill("other.endurance") / 5, 5);
}

/// \return The amount of hb for a single point of mana to regenerate.
int query_mana_heal_delay() {
   return max(50 - query_stat("emp")
                 - query_stat("cha")
                 - query_skill("magic.concentration") / 15
             - query_skill("magic.meditation") / 25, 10);
}

/// \return The amount of hb for a single point of endurance to heal.
int query_end_heal_delay() {
   return max(21 - query_stat("con")
                 - query_skill("other.endurance") / 7, 1);
}

/** \return A non-zero integer will be returned if any of the body-parts
 * given by \a bp match the body parts possessed by this living object;
 * the integer returned is all the body parts this living actually has,
 * or'd together.
 */
int query_has_bp( int bp ) {
   object ob;

   ob = query_race_object();
   if( ob ) return ob->query_bp_flags() & bp;
   return 0;
}

/** \return The name of the body part given by the flag \a bp.
 * If an invalid flag is passed in (you may not OR them together)
 * then 0 is returned; 0 is also returned if the creature does
 * not possess the given body part.
 */
string query_bp_name( int bp ) {
   object ob;
   if( !query_has_bp( bp ) )
      return 0;
   ob = query_race_object();
   return ob->query_bp_name( bp );
}

/** \return An aspect of this object's alignment.
 * \param x Determines which aspect of the alignment you wish to know. Possible values are:
 *  - 0 (or no value specified) -- Good/evil alignment
 *  - 1 -- Chaotic/lawful alignment
 *  - 2 -- Any custom alignment information (if 0 is returned then no custom alignment is set).
 */
int query_alignment(int x) { return alignment[x]; }

int query_good_alignment() {
   return query_alignment( 0 );
}

int query_lawful_alignment() {
   return query_alignment( 1 );
}

mapping query_favor_map() {
   if (!mappingp(favor))
      favor = ([]);
   return favor;
}
int query_favor(string god) {
   return query_favor_map()[FAITH_DAEMON->query_god_name(god)];
}

/** \return An integer indicating a player's level of ability in a given skill. If no
 * value is set for the skill, it will return 0.
 */
int query_skill_level(string x ///< The name of the skill whose level you wish to know.
  ) {
  if( !skills ) { skills = ([]); return 0; }
  return skills[x, 0];
}

/** \return An integer indicating an object's actual bonus for a particular skill.
 */
int query_skill(string skill) {
   int level, *stat = ({}), base, total;
   float bonus;
   string str;
   if (!SK_DAEMON->query_skill_exists(skill)) {
      debug("attempt to query info on nonexistent skill '"+skill+"' from "+object_name(previous_object()),"combat");
      return 0;
   }
   if( !skills ) skills = ([ ]);
   return skills[skill, 0];
}

int query_modified_skill( string skill ) {
   string istat, *stats;
   int skill_base, bonus;

   skill_base = query_skill( skill );
   bonus = 0;
   stats = SK_DAEMON->query_skill_stats( skill );
   foreach( istat : stats )
      bonus += query_stat(istat) * 2 - 20;
   if( sizeof(stats) )
      bonus /= sizeof(stats);

   return max( min(100, skill_base + bonus), 0 );
}

/** \return The cost to advance a skill to \a level
 */
int query_skill_cost(string skill, int level) {
  int x, cost;
  if (level <= 1)
    return level;
  else
    if (SK_DAEMON->query_skill_exists(skill))
      cost = SK_DAEMON->query_skill_difficulty(skill);
    else
      cost = 100; // difficulty for nonexistant skill ;)
  cost = cost * level * (level + 1) / 2;
  // debug("cost of "+skill+" at "+level+" = "+cost,"combat");
  return cost;
}

/** \return the maximum temporary xp a particular skill may have
 */
int query_skill_cap(string skill, int level) {
/*
   int new = query_skill_cost(skill, level);
   int old = query_skill_cost(skill, level-1);
   float cap = ((new - old) * (1.01 - (0.01 * level)));
   // debug("skill cap for "+skill+" at "+level+" = "+cap,"combat");
   return (int)cap;
*/
   //debug("recursion check, calling query_skill_cap(" + skill + ", " + level + ")", "gp");
   return query_skill_cost(skill, level) + 500; // Saner behavior (ask Acius if wondering why)
}

/** \return The total xp earned in a given skill.
 */
int query_skill_total_exp(string x ///< The name of the skill in question
  ) {
  if ( !skills ) { skills = ([]); return 0; }
  return skills[x, 1];
}

/** \return The current amount of xp in a given skill (waiting to be
 * assimilated.
 */
int query_skill_current_exp(string x ///< The name of the skill in question
  ) {
  if ( !skills ) { skills = ([]); return 0; }
  return skills[x, 2];
}

/** \return The maximum value that may be stored in skills[x,2]
 * \note this is really just a lazy alias for query_skill_cap...
 */
int query_skill_max_exp(string x ///< The name of the skill in question
  ) {
  if ( !skills ) { skills = ([]); return 0; }
  return query_skill_cap(x,skills[x,0]+1);
}

/** \return The entire mapping of a player's skills. */
mapping query_skills() {
   if( !skills ) skills = ([]);
   return skills;
}

/** Returns a string giving the alignment of this living object.
 * If \a align is passed in (optional) the resulting string
 * is based on the alignment information it contains.
 */
varargs string query_alignment_string( mixed *align ) {
  string buf;

  if( !pointerp(align) || sizeof(align) < 3 ) align = alignment;
  if( !pointerp(align) || sizeof(align) < 3 ) return "none";

  if (align[2])
    buf = align[2];
  else {
    if (align[1] >= 350)
      buf = "lawful";
    else  if (align[1] >= -350)
      buf = "neutral";
    else
      buf = "chaotic";

    if (align[0] >= 350)
      buf += " good";
    else if (align[0] >= -350) {
      if (buf != "neutral")
        buf += " neutral";
      else
        buf = "true neutral";
    } else
      buf += " evil";
  }
  return buf;
}

/// \return A pointer to this living's current combatant; 0 if it has none.
mixed query_target() {
   if( !target || !target[0] ) return 0;
   return present( target[0], environment(this_object()) );
}

/// \return An array of current targets for combat (all that I am mad at).
mixed query_targets() {
   return target;
}

/// \return True if this living is fighting targ
int is_fighting( object who ) {
   if( !sizeof(target) )
     return 0;
   return member( target, who) != -1;
}

/// This function sets the entire target queue
void set_targets( object* new_targets ) {
    if (pointerp(new_targets))
      target = copy(new_targets);
}

/// \return True if this living is currently in combat mode.
int query_combat_mode() { return combat; }

/// \return The amount of money this creature possesses. Somewhat expensive, don't use wantonly.
int query_money() {
   object ob;
   int val;

   foreach( ob : deep_inventory(this_object()) )
      if( ob->query_is_money() ) val += ob->query_value();
   return val;
}

/// \return True if this living object has outstanding targets in the same room, false otherwise.
int query_in_combat() {
   drop_invalid_targets();
   return pointerp(target) && sizeof(target) > 0 && environment(target[0]) == environment() && target[0] != this_object();
}

/// This living's maximum hit points (which is derived from its constitution).
int query_max_hp() {
  object ob = query_race_object();
  return query_skill("other.endurance") / 4 +
         query_constitution() * ob->query_hp_per_con() +
         ob->query_min_hp();
}

/// Alias for query_max_hp()
int query_maxhp() { return query_max_hp(); }

/// This living's maximum mana
int query_max_mana() {
  object ob = query_race_object();
  int total = query_intelligence() * ob->query_mana_per_int() +
              query_empathy() * ob->query_mana_per_emp() +
              ob->query_min_mana();
  string mtype, *aff = ob->query_mana_affinities();
  if( !aff )
     aff = ({});
  foreach( mtype : MAGIC_DAEMON->query_mana_types() ) {
     int idx = member( aff, mtype );
     total += query_skill("magic.mana."+mtype) / (idx==-1?6:1+idx);
  }
  return total;
  /* Old human values
   *
   *     query_skill("magic.mana.chaos") / 8 +
   *     query_skill("magic.mana.dark") / 8 +
   *     query_skill("magic.mana.earth") / 2 +
   *     query_skill("magic.mana.lightning") / 4 +
   *     query_skill("magic.mana.fire") / 4 +
   *     query_skill("magic.mana.ice") / 4 +
   *     query_skill("magic.mana.order") / 4 +
   *     query_skill("magic.mana.spirit") / 2 +
   *     query_skill("magic.mana.water") / 4 +
   *     query_skill("magic.mana.wind") / 4 +
   */
}

/// Alias for query_max_mana()
int query_maxmana() { return query_max_mana(); }

/// This living's maximum endurance
int query_max_endurance() {
  object ob = query_race_object();
  return query_will() * ob->query_end_per_will() +
         query_constitution() * ob->query_end_per_con() +
         query_skill("other.endurance") +
         ob->query_min_end();
}

/// Alias for query_max_endurance()
int query_maxend() { return query_max_endurance(); }

/// \return If name is specified, whether they are trusted otherwise, returns the entire trusted list.
varargs mixed query_trust( string x ) {
   if (!trust || dead)
      return 0;
   if (x)
      return member(trust,x);
   return trust;
}

/// \return A mapping of all spells this living knows
mapping query_known_spells() {
   return known_spells;
}

/// \return A list of all crafting recipes this living knows
string *query_known_recipes() {
   return known_recipes;
}

/// \return True if the living knows the recipe specified
int query_recipe(string str) {
   return member(known_recipes, str) != -1;
}

/// \return A mapping of all the forms this living knows
mapping query_known_forms() {
   mapping temp;
   string form;
   mixed x;
   int k;
   if( !known_forms || sizeof(known_forms) == 0 )
     known_forms = ([ "brawl":0 ]);
   if (widthof(known_forms) != 1) {
     debug(query_name()+" has misshapen forms mapping. Fixing.","combat");
     temp = m_allocate(sizeof(known_forms),1);
     foreach (form: m_indices(known_forms))
        temp[form] = 0;
     known_forms = temp;
   }
   return known_forms;
}

/// \return The form currently being used by this player.
string query_form() {
   return curr_form;
}

/// \return The object pointer for the form you are currently using.
varargs object query_form_object( string form ) {
   if( form ) return load_object( FORMS_DIR + form );
   return load_object( FORMS_DIR + curr_form );
}

/** \return The action at position \a x in the queue. Position 0 is the front
 * of the queue. If the position specified is invalid, returns 0.
 */
mixed *query_action(int x) {
  if( !intp(x) ) return 0;
  if( !pointerp(action) ) action = ({ });
  if( x < sizeof(action) && x >= 0 )
    return action[x];
   return 0;
}

/// Returns the name of action x in the queue
string query_action_name(int x) {
  mixed a = query_action(x);
  string buf;
  if (!a || !a[B_FNAME])
    return "<error>";
  buf = a[B_FNAME]->query_action_name(a);
  if (!buf)
    if (stringp(a[B_FNAME]))
       buf = "<"+explode(a[B_FNAME],"/")[<1]+">";
    else if (objectp(a[B_FNAME]))
       buf = "<"+a[B_FNAME]->query_name()+">";
  return buf;
}

/// Returns the array of currently pending battle actions.
mixed *query_actions() {
   if( !pointerp(action) ) action = ({ });
   return action;
}

string *query_form_actions( string form ) {
   if( !form ) form = curr_form;

   if( !pointerp(known_forms[form]) && file_exists(FORMS_DIR + form + ".c") ) {
      string *actions;
      actions = (FORMS_DIR + form)->query_default_actions();
      if( !actions )
        return 0;
      known_forms[form] = actions;
   }
   return known_forms[form];
}

/// \return True, if \a act is an enabled action/reaction within the current form.
int query_action_enabled( string act ) {
   string *acts;

   acts = query_form_actions( curr_form );
   if( !acts ) return 0;
   return member( acts, act ) >= 0;
}


// These are overriden from desc to allow for corpses.

DISTANT( "a youthful critter" )
SPECIFIC( "the youthful critter" )
PLURAL( "youthful critters" )

/// \return The living's distant, if alive, otherwise the distant plus "'s corpse"
string query_distant() {
   if( is_corpse ) return ::query_distant() + "'s corpse";
   else return ::query_distant();
}

/// \return The living's plural, if alive, otherwise "corpses of [plural]"
string query_plural() {
   if( is_corpse ) return "corpses of " + ::query_plural();
   else return ::query_plural();
}

/// \return The living's alt_plural(s), adding in "corpses" if it's dead.
string *query_alt_plural() {
   if( is_corpse ) return ::query_alt_plural() + ({ "corpses" });
   else return ::query_alt_plural();
}

/** \return The living's specific description, if alive, otherwise a
 * brief description of its corpse.
 */
string query_specific() {
   if( is_corpse )
      return "the corpse of " + ::query_distant();
   else
      return ::query_specific();
}

/// The string you get when you look at this living; varies if it is dead.
string query_look() {
   string corpse_str;
   if( dead ) {
      corpse_str = "This is "+query_specific()+". ";
      if( dead < 50 ) corpse_str += "It looks as though it died quite recently.?(HASINV: On the corpse, you see $(INV).)";
      else if( dead < 100 ) corpse_str += "It exudes a faint aroma of decay.?(HASINV: On the corpse, you see $(INV).)";
      else if( dead < 150 ) corpse_str += "The stench from it is almost overwhelming, and it is covered in the buzzing of flies.?(HASINV: On the corpse, you see $(INV).)";
      else corpse_str = "This is the skeleton of " + ::query_distant() +".?(HASINV: In the bones, you see $(INV).)";
      return corpse_str;
   }
   else return ::query_look();
}

/// Only true if this object is dead (corpses may be picked up).
int query_gettable() {
   if( is_corpse ) return 1;
   else return ::query_gettable();
}

/// \return True. Living objects may always be dropped. See query_gettable().
int query_droppable() {
   return 1;
}

/// \return A race object, even if no valid race is set...
object query_race_object() {
   if( file_exists(RACE_DIR + race + ".c") )
      return load_object( RACE_DIR + race );
   //debug((string)this_object()+" does not have a valid race ("+race+"), using generic"); // Commented out by Ben 8/14 to reduce spam
   return load_object( RACE_DIR + "generic" );
}

/// \return The full path to the race object for this living object.
string query_race() { return race; }

/** \return The default_doing string (the string returned by
 * query_doing when you're not busy doing anything).
 */
string query_default_doing() {
   if( !default_doing )
   switch( query_stance() ) {
      default: return "standing here";
      case ST_SITTING: return "sitting here";
      case ST_CROUCHING: return "crouching here";
      case ST_JUMPING: return "jumping through the air";
      case ST_FALLING: return "falling";
      case ST_FLYING: return "flying";
      case ST_UNBALANCED: return "falling over";
   }
   return default_doing;
}

/** \return A processed version of the doing string. All grammar codes are
 * evaluated using the target passed into set_doing(); ANSI codes will not
 * be evaluated. This function does call set_target, but it restores the
 * old target when it is done, so you shouldn't have problems with side-effects.
 * Make sure you have called set_listener before you call this function,
 * otherwise point-of-view can't be evaluated properly.
 */
string query_doing() {
   object old_targ, old_actor;
   string str;

   // Don't upset the currently set target.
   old_targ = query_target();
   old_actor = query_actor();
   set_target( doing_target );
   set_actor( this_object() );
   // Only parse grammar codes now.
   if( doing )
      str = process_string(doing, 0, 1);
   else
      str = process_string(query_default_doing(), 0, 1);
   set_target(old_targ);
   set_actor(old_actor);
   return str;
}

/// Return the particular
string query_threat_action() {
  return this_object()->query_env_var("TthreatAction");
}

/** Determine whether or not this living threatens (x,y)
 * Notes: IMO this function would do better to return a mapping
 * of all the points threatened by this object. That makes it
 * quite a bit quicker, and allows you to optimize the walk
 * command if you want to. As it stands, this function doesn't
 * allow for unusually shaped paths in any case (only can
 * threaten adjacent squares atm). Also, returning a mapping
 * of threatened points simplifies the call a bit (no params).
 */
mapping query_threaten(mapping path) {
  int loc;
  if( !mappingp(path) ) return 0;
  debug("query_threaten called on "+(string)this_object(),"combat");
  foreach (loc: m_indices(path)) {
    if (query_doing() == "watching") {
      if (this_object()->query_env_var("watch_loc") == loc)
        return ([loc:this_object()]);
    } else {
      int tx, ty;
      tx = abs(this_object()->query_x()-CX(loc));
      ty = abs(this_object()->query_y()-CY(loc));
      debug("query_threaten: tx = "+tx+", ty = "+ty,"combat");
      if ( (ty == 1 && tx == 0) || (tx == 1 && ty == 0) )
        return ([loc:this_object()]);
    }
  }
  return ([]);
}

/** Perform action found in environment variable TthreatAction on
 * occupant \a victim of threatened square \a x, \a y.
 * \return 1 if movement is stopped.
 */
int on_threaten(object victim, int x, int y) {
  string act = this_object()->query_threat_action();
  object *weapons, weapon;
  debug("on_threaten("+(string)victim+","+x+","+y+") called on "+(string)this_object(),"combat");
  if( !act || !victim || !(victim->query_is_living()) || victim == this_object() )
    return 0;
  set_target(victim);
  switch (act) {
    case "block":
      msg_local("~CACT~Name ~verbprevent ~targ from going any further.~CDEF");
      readiness -= 20;
      return 1;
    case "attack":
      if (weapons = BATTLE_DAEMON->query_wielded_weapons(this_object()))
        weapon = weapons[random(sizeof(weapons))];
      if (!weapon)
        weapon = this_object();
      msg_local("~CACT~Name ~verbattack ~targ as ~prot ~vertpass!~CDEF");
      this_object()->execute_melee(weapon,weapon->query_wc(),victim,20,ATT_NO_BLOCK|ATT_NO_DODGE);
      readiness -= 50;
      return 0;
  }
  return 0;
}

/** If this object is dead, returns true if \a str identifies this corpse. Otherwise
 * uses the standard id() in desc
 */
object id(string str) {
   if( is_corpse )
   if( str == "corpse" || str == "corpses" || str == query_distant() || str == query_specific() || str == query_plural() )
      return this_object();
   return ::id(str) || str == this_object()->query_cap_name();
}

/// Returns list of this objects followers
object *query_followers() { return my_followers; }
/// Returns list of this objects leaders
object *query_leaders() { return my_leaders; }

/** (Hack) rerolls the player's stats. This should be moved from the bin verb to here, IMO.
 * (Acius: Sigh, in WHOSE opinion? I don't actually care, btw ... actually, I disagree. Verbs
 * are easy to update, living is dangerous and stupid to update. Never put anything in living
 * unless you have to)
 */
void reroll() {
   "/bin/wiz/reroll"->main(name);
}

int query_touchable( object ob ) {
   mixed shape, border;

   if( !objectp(ob) ) return 0;
   if( query_adjacent(ob) ) return 1;
   if( member(all_environment(ob), this_object()) >= 0 ) return 1;
   shape = ob->query_shape();
   border = ob->query_border();
   if( mappingp(shape) && member(shape, this_object()->query_coord() + MAKE_C(4096, 4096, 32)) )
      return 1;
   if( mappingp(border) && member(border, this_object()->query_coord() + MAKE_C(4096, 4096, 32)) )
      return 1;

   if( !mappingp(shape) && !mappingp(border) )
      return ob->query_adjacent( this_object()->query_x(), this_object()->query_y() );

   return 0;
}

/******************************************************************** SETS */
///Sets up the desc variable;
void setup_desc_var() {
   mapping desc = ([]);
   desc["hair_color"] = "bland";
   desc["hair_length"] = "bland";
   desc["hair_style"] = "bland";
   desc["eye_color"] = "bland";
   desc["skin_color"] = "bland";
   desc["adj"] = "bland";
   this_object()->set_env_var("desc", desc);
}
/// Sets the entire mapping of names/people that this player has been introduced to.
void set_names_known( mapping m ) { names_known = m; }

/// Adds a name to the list of people/things you recognize.
void add_recognize( string name ) {
   if( !mappingp(names_known) ) names_known = ([ ]);
   names_known += ([ lower_case(name) ]);
}

/// Removes a name from the list of people/things you recognize.
void remove_recognize( string name ) {
   if( mappingp(names_known) )
      names_known -= ([ lower_case(name) ]);
}

/// Sets the entire skills mapping.
void set_skills( mapping m ) {
   if( mappingp(m) )
      skills = m;
}

/// This object's current battle stance.
void set_stance(int x) { stance = x; }

/// Change this living's movement verb.
void set_movement_type(string str) {
   this_object()->set_env_var("movement_type",str);
}

/** Change stance and impose a readiness penalty when doing so. This is
 * used when the living is voluntarily changing stances, not when being
 * knocked around... The costs used here are my second draft, we'll see
 * how they do.
 * \return whether or not the stance change was successfull.
 */
int change_stance(int x) {
   int cost;
   string buf;
   if (stance == x)
      return -1;
   if (endurance < 1)
      return 0;
   switch (stance) {
      case ST_STANDING:
         switch (x) {
            case ST_SITTING:
               buf = "~Name ~verbsit down.";
               cost = 100; break;
            case ST_LYING:
               buf = "~Name ~verblie down.";
               cost = 150; break;
            case ST_CROUCHING:
               buf = "~Name ~verbdrop into a crouch.";
               cost = 50;  break;
            case ST_JUMPING:
               buf = "~Name ~verbjump.";
               cost = 100; break;
         }
         break;
      case ST_SITTING:
         switch (x) {
            case ST_STANDING:
               buf = "~Name ~verbstand up.";
               cost = 150; break;
            case ST_LYING:
               buf = "~Name ~verbroll onto ~poss back.";
               cost = 50;  break;
            case ST_CROUCHING:
               buf = "~Name ~verbrise into a crouch.";
               cost = 100; break;
            case ST_JUMPING:
               return 0;   // can't jump from seated
         }
         break;
      case ST_LYING:
         switch (x) {
            case ST_STANDING:
               buf = "~Name ~verbstagger to ~poss feet.";
               cost = 200; break;
            case ST_SITTING:
               buf = "~Name ~verbsit up.";
               cost = 100; break;
            case ST_CROUCHING:
               buf = "~Name ~verbrise into a crouch.";
               cost = 100; break;
            case ST_JUMPING:
               return 0;   // definately can't jump while prone
         }
         break;
      case ST_CROUCHING:
         switch (x) {
            case ST_STANDING:
               buf = "~Name ~verbstand back up.";
               cost = 50;  break;
            case ST_SITTING:
               buf = "~Name ~verbsit down.";
               cost = 50;  break;
            case ST_LYING:
               buf = "~Name ~verblie down.";
               cost = 100; break;
            case ST_JUMPING:
               buf = "~Name ~verbleap from ~poss crouch.";
               cost = 50;  break;
         }
         break;
      case ST_UNBALANCED:
         switch (x) {
            case ST_STANDING:
               buf = "~Name ~verbregain ~poss balance.";
               cost = 100; break;
            case ST_SITTING:
               buf = "~Name ~verbfall down.";
               cost = 50;  break;
            case ST_LYING:
               buf = "~Name ~verbfall prone.";
               cost = 50;  break;
            case ST_CROUCHING:
               buf = "~Name ~verbbend ~poss knees.";
               cost = 100; break;
            case ST_JUMPING:
               buf = "~Name ~verbjump.";
               cost = 150; break;
         }
         break;
      // You never can manually shift stances from one of these
      case ST_JUMPING:
      case ST_FALLING:
      case ST_FLYING:    return 0;
   }
   if (readiness < cost)
      return 0;
   if (random(300) < cost)
      endurance--;
   readiness -= cost;
   set_stance(x);
   set_actor(this_object());
   this_object()->msg_local("~CACT"+buf+"~CDEF");
   return 1;
}

/** Set this living's current readiness, which describes how ready they
 * are to perform an action. Turns on the heart beat.
 */
void set_readiness(int x) { readiness = x; set_heart_beat(1); }

/// Set this living's "held" timer (paralysis). Turns on the heart beat.
void set_held(int x) {
   if( held && x == 0 ) {
      set_actor(this_object());
      if( !query_dead() )
         this_object()->msg_local("~CACT~Name ~verbis no longer paralyzed.~CDEF");
   } else if( !held && x ) {
      set_actor(this_object());
      if( !query_dead() )
         this_object()->msg_local("~CACT~Name ~verbis paralyzed!~CDEF");
   }
   held = x;
   set_heart_beat(1);
}

/// Set this living's "busy" timer (occupied w/ spell, etc., no actions). Turns on heart beat.
void set_busy(int x) { busy = x; set_heart_beat(1); }

/// Sets this living's wimpy percentage.
void set_wimpy(int x) { if (x >= 0 && x <= 100) wimpy = x; }

/// Sets this living's soak preference (on or off)
void set_soak(int x) { soak = (x?1:0); }

/// Sets entire list of people to trust.
void set_trust( mapping x ) { trust = x; }

/// Add a name to the trust list.
void add_trust( string who ) {
   if( !mappingp(trust) ) trust = ([ ]);
   trust += ([ who ]);
}

/// Removes a name from the ignore list.
void remove_trust( string who ) {
   if( mappingp(trust) )
      trust -= ([ who ]);
}

void set_known_recipes(string *arr) { known_recipes = arr; }

void add_recipe(string str) {
   if( !pointerp(known_recipes) )
      known_recipes = ({});
   if( member(known_recipes, str) != -1 )
      return;
   known_recipes += ({ str });
}

void remove_recipe(string str) {
   if( !pointerp(known_recipes) )
      known_recipes = ({});
   known_recipes -= ({ str });
}

void set_known_spells( mapping m ) {
   if (!mappingp(m)) return;
   if (m == ([]) || widthof(m) == 1)
      known_spells = m;
}

varargs void add_spell( mixed a, mixed b ) {
   if( member(query_known_spells(), a) && !b )
      return;
   if (stringp(a))
      known_spells[a] = (b?b:1);
   else if (mappingp(a))
      set_known_spells(a + query_known_spells());
}

varargs void add_known_spell( mixed a, mixed b ) {
   debug("add_known_spell is deprecated, use add_spell in stead");
   add_spell( a, b );
}

varargs void practice_spell(string name, int x) {
   if (x <= 0)
      x = 1;
   known_spells[name] += x;
}

/** Enables the action \a act for the form \a form.
 * \return True if the action was enabled, and false if you don't have the form or it was already enabled.
 */
int add_form_action( string form, string act ) {
   if( !pointerp(known_forms[form]) ||
      member(known_forms[form], act) >= 0 ||
      member(query_form_object(form)->query_all_actions(), act) < 0 )
      return 0;
   known_forms[form] += ({ act });
   return 1;
}

void remove_form_action( string form, string act ) {
   if( !pointerp(known_forms[form]) ) return;
   known_forms[form] -= ({ act });
}

void set_combat_mode(int x) {
   if (x == 0 || x == 1)
       combat = x;
}

/** Changes the form currently used by this creature. This should be a string
 * giving an object name in the forms directory (FORMS_DIR, a constant found in
 * const/battconst.h). The forms directory is implied, so if you have a file
 * "/battle/forms/brawl.c" then you can set_form("brawl").
 *
 * There are major consequences to passing in objects that are broken, so
 * a load_object is performed as a sanity check.
 *
 * This function used to perform messaging, which is kind of a violation of
 * the semantics of a set function. To do a form change message, use this
 * line (or a variation that adds color, or whatever):
 *
 * this_player()->msg_local( this_player()->query_form_object()->query_change_message() );
 */
varargs void set_form( string x ) {
   string message;
   if (stringp(x))
      curr_form = x;
   else
      curr_form = "brawl";
   message = query_form_object()->query_change_message();
   // Clear the action queue
   if( query_action(0) ) {
      (query_action(0)[B_FNAME])->on_interrupt(query_action(0));
      set_actions( 0 );
   }
}

/** Directly sets the mapping of known_forms. See the docs on known_forms
 * for structure information.
 */
void set_known_forms( mapping m ) {
   if( mappingp(m) )
      known_forms = m;
}

/// Add a new form to the living's list. Checks if it's valid first.
void add_form( string str ) {
   string *base_actions, *base_skills, iskill;

   // If I have the form already, do nothing.
   if( known_forms[str] ) return;
   if( file_exists(FORMS_DIR + str + ".c") ) {
      base_actions = (FORMS_DIR + str)->query_default_actions();
      known_forms[str] = base_actions;
      if( !base_actions ) return;
      base_skills = (FORMS_DIR + str)->query_default_skills();
      if( !base_skills ) return;
      foreach( iskill : base_skills )
         this_object()->add_skill( iskill );
   }
}

/// Remove a form from the living's list of known forms.
void remove_form(string str) { m_delete( known_forms, str ); }

void del_form(string str) { remove_form(str); DEP("del_form", "remove_form"); }

/** Returns a form that is usable with the currently wielded equipment. 0 if
 * no forms are possible.
 */
varargs string get_best_form() {
   object ob, *wlist;
   mapping allowed_forms = copy(known_forms);
   string str, best = 0;
   int bestsize = -1;

   wlist = BATTLE_DAEMON->query_wielded_weapons( this_player() );

   // determine forms allowed by weapon(s) wielded
   if( wlist ) {
      foreach (ob: wlist) {
         foreach( str: m_indices(allowed_forms) ) {
           if( member((FORMS_DIR+str)->query_weapon_types(), ob->query_weapon_type()) == -1 ) {
              allowed_forms -= ([ str ]);
           }
         }
      }
   } else {
      foreach (str: m_indices(allowed_forms))
         if( member((FORMS_DIR+str)->query_weapon_types(), "combat.weapon.unarmed") == -1) {
             allowed_forms -= ([ str ]);
         }
   }

   // Determine best form. Ideally, this function would also try
   // and pick the best one.
   foreach (str: m_indices(allowed_forms))
      best = str;

   return best;
}

/** Sets your current form to whatever the return value of get_best_form is; if
 * no form is possible, unwields all weapons and tries again. If that doesn't work,
 * adds the brawl form and sets you to that. This function does do some
 * messaging.
 */
void adopt_best_form() {
   string best = get_best_form();
   string old_form = query_form();
   object ob;

   object *wlist = BATTLE_DAEMON->query_wielded_weapons( this_player() );
   if( !best && wlist ) {
      msg( "~CWRNYou do not know any forms that use " +
         inventory_string(map(wlist,(: $1->query_distant() :))) +
         ".~CDEF" );
      foreach( ob : all_inventory(this_object()) ) {
         if( !ob->query_cursed() )
            ob->set_held( 0 );
      }
      best = get_best_form();
   }
   if( !best ) {
      add_form( "brawl" );
      best = "brawl";
   }
   set_form( best );
   if( best != old_form )
      msg_local( "~CACT" + query_form_object()->query_change_message() + "~CDEF" );
}

/// Set the number of heart beats it takes for hit points to increase a point.
void set_heal_delay( int x ) {
  debug("set_heal_delay() is now deprecated as the desired value is now "+
        "calculated automatically from stats and endurance skill.");
}

/// Set the number of heart beats for mana to increase a point.
void set_mana_heal_delay( int x ) {
  debug("set_mana_heal_delay() is now deprecated as the desired value is now "+
        "calculated automatically from stats and spellcraft skill.");
}

/// Set the number of heart beats for endurance to increase a point.
void set_end_heal_delay( int x ) {
  debug("set_end_heal_delay() is now deprecated as the desired value is now "+
        "calculated automatically from stats and endurance skill.");
}

/// Sets the default_doing string, which is "what you do when you're not doing anything."
void set_default_doing( string s ) {
   default_doing = s;
}

/** Sets the "doing" string, which determines what the player is currently doing.
 * The doing string should be a gerund, which means a phrase using an -ing verb.
 * Good examples are "smelling the roses", "pacing back and forth", "sitting on
 * the ground", or "being eaten alive by wolves".
 * \param x The doing string to set
 * \param targ (Optional) A target object used in the doing string. It will
 * be evaluated every time query_doing is called.
 */
varargs void set_doing( string x, object targ ) {
   doing = x;
   doing_delay = DOING_DELAY;
   doing_target = targ;
   set_heart_beat( 1 );
}

/// Set this living's total hit points. No range checking is done. Heart beat is enabled (for healing).
void set_hp(int x) {
   hp = x;
   set_heart_beat( 1 );
}

/// Set this living's magic points. No range checking. Heart beat enabled.
void set_mana(int x) { mana = x; set_heart_beat(1); }

/// Set this living's endurance. No range checking. Heart beat enabled.
void set_endurance(int x) { endurance = x; set_heart_beat(1); }

/// A "shortcut" function for set_endurance.
void set_end(int x) { set_endurance(x); }

/// Set this living's food level.
void set_food(int x) {
  food = x;
  recalc();
}

/// Set this living's drink level.
void set_drink(int x) {
  drink = x;
  recalc();
}

/// Set this living's autoeat value.
void set_autoeat(string x) { this_object()->set_env_var("Tautoeat", x); }

/// Set this living's autodrink value.
void set_autodrink(string x) { this_object()->set_env_var("Tautodrink", x); }

/// Set this living's autotake value.
void set_autotake(int x) { this_object()->set_env_var("Tautotake", x); }

/// Sets the last thing that you referred to as a pronoun.
void set_pronoun( mixed x ) {
   if( !x ) pronoun = ({ ({ }), ({ }), ({ }), ({ }) });
   else {
      if( pointerp(x) )
         pronoun[3] = x;
      else
         if( objectp(x) ) pronoun[x->query_gender()] = ({ x });
   }
}

/** Move an amount of xp from the current (temporary) slot into the
 * total (permanent) slot. Increase skill level if necessary
 * \note that this function is called by decay_food_and_drink() for every
 * food decay cycle.
 */
void assimilate_skill_xp() {
  int level, total, current, amt;
  string skill;
  // Don't assimilate points during combat or while dead
  // update: skill points also do not assimilate as long as you are actually
  //         doing something else
  if (this_object()->query_combat_mode() || this_object()->query_dead() ||
      this_object()->query_action() )
    return;
  foreach( skill : m_indices(skills) ) {
    // debug("assimilating "+this_object()->query_name()+"'s "+skill,"combat");
    level = skills[skill, 0];
    total = skills[skill, 1];
    current = skills[skill, 2];
    // debug("level = "+level+", total = "+total+", current = "+current,"combat");
    // Skip this iteration if there are no points to assimilate
    if ( !current )
      continue;

    // Calculate the amount to move
    amt = level;
    amt += (this_object()->query_intelligence()/4);
    if (stance == ST_LYING)
      amt += 2;
    else if (stance == ST_SITTING)
      amt++;
    if (amt > current)
      amt = current;

    // Do the move
    skills[skill, 1] += amt;
    skills[skill, 2] -= amt;

    // Check if the skill level has increased
    if ( skills[skill, 1] >= query_skill_cost(skill, level+1) ) {
//      if ( ("/bin/skills"->query_proficiency(level)) !=
//           ("/bin/skills"->query_proficiency(level+1)) )
        msg_object( this_object(), "~[110You feel that your skill in "+
                    (SK_DAEMON->query_skill_description(skill))+"~[110 has increased slightly.~CDEF" );
      set_skill(skill, level+1);
    }
  }
}

#define CHO { object iho; write("-> "); foreach(iho:all_inventory()) if(iho->id("hunger_ob")) write(as_string(iho)); write( "<-\n"); }

/// Decrease this living's food and drink level
void decay_food_and_drink() {
   int f, d, x, y, age = 60, show_message, mult;
   string buf, fbuf, dbuf;
   object race_ob = query_race_object();
   object buff_ob = present_clone( "/obj/foodbuff", this_object() );

   // make sure hunger values are integers
   food = to_int( floor(food) );
   drink = to_int( floor(drink) );

   if( dead )
      return;

   // tick food buff
   if( buff_ob )
      buff_ob->on_tick();

   // precalc multiplier, saving many many cycles...
   // then save players from meltdown for daemon oopsitude
   mult = to_int(min(50, floor(age / 60)));

   // decrease their crafting limits here ;)
   mapping crafting_cap = this_object()->query_env_var("crafting_cap");
   if ( mappingp(crafting_cap) ) {
      foreach ( buf : m_indices(crafting_cap) ) {
         x = max(1, crafting_cap[buf] / 2);
         crafting_cap[buf] -= x * mult;
         if ( crafting_cap[buf] < 1 )
            crafting_cap -= ([ buf ]);
      }
      this_object()->set_env_var("crafting_cap", crafting_cap);
   } else
      this_object()->set_env_var("crafting_cap", ([]) );
   // end: cap stuff

   // never debit food more frequently than once every 60 seconds
   if (age >= 60) {
      f = race_ob->query_food_cost();
      d = race_ob->query_water_cost();
      if (stance == ST_LYING) {
         f = f * 2 / 3;
         d = d * 2 / 3;
      } else if (stance == ST_SITTING) {
         f = f * 3 / 4;
         d = d * 3 / 4;
      } if ((100 * food / query_food_capacity()) < 25) {
         f--;
         this_object()->add_weight(-15 * mult);
      }
   }
   /** this call makes absolutely no sense to me right now...
    *
    * if ((100 * food / query_food_capacity()) > 110)
    *  this_object()->add_weight(5 * mult);
    */
   food -= f * mult;
   drink -= d * mult;

   if (random(5) == 0) {
      show_message = 1;
   }
   // Update skill values, this is a BAD way to do this, but until I move
   // these functions elsewhere, this is how I'm going to do it. - al
   assimilate_skill_xp();

   if( food < 0 || drink < 0 ) {
      // recalculate bonuses if starving
      recalc();
      // always show messages when players are starving
      // gp:  Also show for hungry mobs
      show_message = 1;
   }

   // Inform them of their food status every 5 food ticks
   // Note: monsters don't have a query_decay_time()
   // function, so don't get messages.
   x = query_food_percent();
   y = query_drink_percent();
   if (x > 25 && y > 25)
      return;

   if( x < 0 || y < 0 ) {
      object ob;

      foreach( ob : all_inventory() ) {
         if( !ob || !ob->id("hunger_ob") ) continue;
         ob->on_hunger();
      }
   }

   if( x < 25 ) this_object()->on_hunger();
   if( y < 25 ) this_object()->on_thirst();

   if( show_message ) {
      // Determine string for hunger status
      if (x < -10)
         fbuf = "starving to death";
      else if (x < 5)
         fbuf = "starving";
      else if (x < 10)
         fbuf = "extremely hungry";
      else if (x < 15)
         fbuf = "getting very hungry";
      else if (x < 20)
         fbuf = "hungry";
      else if (x <= 25)
         fbuf = "slightly peckish";

      // Determine string for thirst status
      if (y < -10)
         dbuf = "dying of thirst";
      else if (y < 5)
         dbuf = "dehydrated";
      else if (y < 10)
         dbuf = "parched";
      else if (y < 15)
         dbuf = "getting very thirsty";
      else if (y < 20)
         dbuf = "thirsty";
      else if (y <= 25)
         dbuf = "a little dry in the throat";

      if( fbuf && dbuf )
         buf = "You are "+fbuf+" and "+dbuf;
      else
         buf = "You are "+(fbuf?fbuf:dbuf);

      if( x < 10 || y < 10 ) buf += "!";
         else buf += ".";

      set_actor(this_object());
      msg_object(this_object(),"~CTIT"+buf+"~CDEF");

      if( x < 20 || y < 10 )
         this_object()->msg_local("~CACT~Npos stomach gurgles loudly.~CDEF");

      // check for interruption
      if (pointerp(query_action(0))) {
         object actobj = query_action(0)[B_FNAME];
         mixed  actpar = query_action(0)[B_PARAM];
         string actnam = query_action_name(0);
         int dc = 0;
         // chance of interruption due to hunger
         if (x < 25)
            dc += 30 - x;
         if (y < 25)
            dc += 30 - y;
         dc *= 2;
         // actually check if the action is interruptable
         if (dc && actobj->query_is_interruptable()) {
            int check = skill_check("magic.concentration",dc,dc + 10);
            //debug("interruption check, dc = "+dc+", check = "+check);
            if (check == 1) {
               this_object()->msg_local("~CACT~Name ~verbstrain to maintain concentration.~CDEF");
            } else if (!check) {
               this_object()->msg_local("~CACT~Name ~verblose ~poss concentration!~CDEF");
               actobj->on_interrupt(query_action(0));
               // dequeue the action
               action = action[1..];
            } // end: concentration check
         } // end: performing an interruptable
      } // end: check for interruption
   } // end: show message

   // trigger autosatiate/drink/eat for players with it set
   if ( (query_drink_percent() <= 25 || query_food_percent() <= 25) &&
      this_object()->query_env_var("Tautosatiate") && query_level() >= WIZLEVEL ) {
      command("satiate "+query_name());
   }
   if( query_drink_percent() <= 25 && query_autodrink() && !query_in_combat() ) {
      this_object()->catch_tell("Autodrink queued.\n");
      command("drink "+query_autodrink());
   }
   if( query_food_percent() <= 25 && query_autoeat() && !query_in_combat() ) {
      this_object()->catch_tell("Autoeat queued.\n");
      command("eat "+query_autoeat());
   }
} // end: decay food and drink

/** \note This function is only included for typo type purposes, use
 * decay_food_and_drink() in stead.
 */
void decay_food() { decay_food_and_drink(); }

/** \note This function is only included for typo type purposes, use
 * decay_food_and_drink() in stead.
 */
void decay_drink() { decay_food_and_drink(); }

/** Set's the full path for this living's race object; This should be a
 * .c file that inherits RACE_DIR + "race". - Actually... this is not the
 * case any more. This string should be simply be set to a string "human",
 * "lizard", etc... where a file "RACE_DIR + s" (that inherits race.c)
 * exists. Am I sufficiently confusing yet? Good. - Al [12.8.02]
 */
void set_race(string s) { race = s; }

/** This function exists primarily for the transition period between the
 * old (random) skills sytem and the new (xp-based) one. It fixes
 * discrepancies in the mapping that stores everything.
 */
void repair_skills_mapping() {
  mapping temp = m_allocate(sizeof(skills), 3);
  string skill;
  int level;

  // make sure the mapping is the right shape
  if (sizeof(skills) != 0) {
    if (widthof(skills) != 3) {
      foreach (skill, level : skills)
        temp += ([skill:level;query_skill_cost(skill,level);
                       0]);
      skills = copy(temp);
    }
  }

  // verify that all values actually stored are good
  foreach (skill : skills) {
    if (!(SK_DAEMON->query_skill_exists(skills[skill,0])))
       skills -= ([ skill ]);
  }
}


void validate_skills() {
  string skill, buf;

  if (widthof(skills) != 4) {
    debug(this_object()->query_name()+" had misshapen skills array. Repairing.","combat");
    repair_skills_mapping();
  }
  foreach ( skill : m_indices(skills) )
     if (!buf && member(skill,'.') == -1) {
        debug(this_object()->query_name()+" had old style skills. Repairing..","combat");
        repair_skills_mapping();
        msg("~CWRNYour skill list has been trimmed. Avoid combat until it can be fixed.~CDEF");
        break;
     }
}


/** Sets the proficiency level of \a skill to \a x. Valid ranges for \a x
 * will vary between implementations, so no validity checking is performed.
 *
 * \param skill Name of the skill being modified
 * \param x The new level for this skill
 *
 * See query_skill().
 */
void set_skill(string skill, int x ) {
  if ( !SK_DAEMON->query_skill_exists(skill) ) {
     debug("attempt to set non-existent skill "+skill+" in "+object_name());
     return;
  }
  validate_skills();

  // important fix here ;)
  if (x == 0)
    x = 1;

  if( sizeof(skills) == 0 ) {
    skills = ([skill:x;query_skill_cost(skill,x);0;query_skill_cap(skill,x)]);
    return;
  } else {
    skills[skill, 0] = x;
    skills[skill, 1] = query_skill_cost(skill, x);
    skills[skill, 2] = 0;
    skills[skill, 3] = query_skill_cap(skill, x);
  }
}

/** Add an amount of xp to a particular skill and make sure that it does
 * not exceed the xp cap for the skill at that particular level.
 *
 * If optional flag is_bonus is set, the xp will be treated as bonus experience
 * in stead of
 */
varargs void add_skill(string skill, int x, int is_bonus) {
  int capped = 0;
  string party;
  if ( !stringp(skill) )
    return;
  if ( !SK_DAEMON->query_skill_exists(skill) ) {
    debug("attempt to set non-existent skill "+skill+" in "+object_name());
    return;
  }
  if ( !query_skill(skill) )
    set_skill(skill, x);
  else  if (skills[skill, 2] + x >= query_skill_max_exp(skill)) {
    if (skills[skill, 2] < query_skill_max_exp(skill))
       msg_object( this_object(), "~CTITYou have learned all you can about " + "/daemon/skill"->query_skill_description(skill) + " for now. Your skill will improve slowly until you take a rest.~CDEF");
    skills[skill, 2] = query_skill_max_exp(skill);
   capped = 1;
  } else {
    skills[skill, 2] += x;
  }

  // handle party distribution of xp
  if( party = this_object()->query_env_var("party") ) {
    //debug( "party = "+party );
    mapping exp = this_object()->query_env_var("party_exp");
    string pupil, member;
    int member_present = 0;
    // make sure there are party members in the room, otherwise, none of this happens
    foreach( member : PARTY_DAEMON->query_party_members(party) ) {
      object ob = find_player(member);
      if( !this_object() || !ob || ob == this_object() )
        continue;
      if( environment(ob) == environment(this_object()) ) {
        member_present = 1;
        // only set pupil if the kid is present
        if( member == this_object()->query_env_var("mentoring") )
          pupil = member;
      }
    }
    if( !member_present )
      return;
    // if it was bonus xp, we just handle it here before being done
    if( is_bonus ) {
      if( pupil ) {
        skills[skill, 2] -= x;
        find_player(pupil)->add_skill(skill, x, 1);
        // after mentor skill hits 50, only way to practice is via generated bonuses
        practice_skill_conditional("other.mentoring", 50, x);
        return;
      } else if( !capped ) {
        return;
      }
      // if you're capped, bonus xp turns around and counts toward generated xp
      // so we go on with the function, otherwise we've returned already
    }
   // some skills don't generate bonuses
   if( SK_DAEMON->query_no_share(skill) )
      return;
    // ok, we have a member, actually accumulate xp now
    debug(skill+": "+x, "party");
    if( !mappingp(exp) )
      exp = ([ ]);
    if( member(exp, skill) && pointerp(exp[skill]) )
      exp[skill][0] += x + ( !pupil && capped );
    else
      exp[skill] = ({ x, 10 });
    // determine if we have generated a bonus
    if( exp[skill][0] >= exp[skill][1] ) {
      int bonus = exp[skill][0] / exp[skill][1];
      exp[skill][0] = exp[skill][0] % exp[skill][1];

      // hack to prevent infinite recursion:  we don't divvy bonus xp
      // -gp
      if (!is_bonus) {
         PARTY_DAEMON->divvy_bonus( party, environment(this_object()), skill, bonus );
      }
      // chance to alter the xp needed to generate the next bonus if they've got
      // a pupil present
      if( pupil )
        exp[skill][1] = 10 - skill_check("other.mentoring",20,40,60,80,100);
    }// end: if we made a bonus
    this_object()->set_env_var("party_exp", exp);
  }// end: if we're in a party
}// end: add_skill()

/** Set this living object's armour class. This is recalculated
 * whenever armour is put on or taken off.
 *
 * There are a couple of ways to use this function:
 *  - set_ac( ([ "blunt":3, "piercing":-2 ]) ); -- this will set
 *    the armour class modifiers directly, and replace any existing modifiers that
 *    may have been in place.
 *  - set_ac( "blunt", 2 ); -- sets the "blunt" modifier to "+2". This will
 *    change a single modifier, without erasing the existing modifiers.
 *  - set_ac( 3 ); -- Sets the base armour class. Does not affect modifiers.
 *    This is the armour class that the modifiers, well, modify.
 */
varargs void set_ac(mixed what, int amt) {
  // we've been passed an entire mapping, replace ac deltas's with it.
  if (mappingp(what)) {
    ac_mod = copy(what);
    return;
  }

  // we've been passed a string..., just set one ac_mod.
  if (stringp(what) && amt) {
    ac_mod[what] = amt;
    return;
  }

  // we've been passed an int, set base ac
  if (intp(what)) {
    ac = what;
    return;
  }
}

void set_base_ac( int x ) {
   ac = x;
}

void set_ac_mod( mapping m ) {
   ac_mod = m;
}

/** Sets the actions array directly. Giving garbage values
 * will cause big, confusing errors, so I don't recommend that.
 */
void set_actions( mixed *x ) {
   action = x;
}

/** Set this living's weapon class. Will be recalculated when
 * weapons are wielded or removed. Passing in 0 will reset
 * the array.
 */
varargs void set_wc(mixed what, int amt) {
  // we've been passed an entire mapping, replace wc with it.
  if (mappingp(what)) {
    wc = copy(what);
    return;
  }

  // we've been passed a string..., just set one wc
  if (stringp(what) && intp(amt)) {
    wc[what] = amt;
    return;
  }
}

/** Replaces one of the commands on the action queue with \a act.
 * If \a x is within the bounds of the action queue, it will replace that element
 * in the queue. If \a x < 0, it will add the action to the front
 * of the queue. If \a x >= the size of the queue, the action goes on the end.
 * \param act A correctly formatted action. See the man page on 'action'
 * This function is particularly useful for adding an action onto the front
 * of the combat queue -- use set_action( -1, whatever );
 */
void set_action( int x, mixed *act ) {
  if( !pointerp(action) ) action = ({ });
  if ( x < sizeof(action) && x >= 0 )
    action[x] = act;
  else if( x >= sizeof(action) )
    action = action + ({ act });
  else if( x < 0 )
    action = ({ act }) + action;
}

/** Pops and performs the action at the top of the queue, but
 * only if this living is ready to perform it.
 *
 * \return
 *  - 0: Either there is no action that needs doing, or
 *       the action could not be performed because this living
 *       is not yet ready (stance change, readiness req, etc.).
 *  - 1: An action was popped and performed.
 *  - 2: An action was popped, but was no longer valid. This
 *       can usually be treated the same as case 1, but there
 *       may be a reason that you need to know the difference.
 */
int pop_action() {
   mixed *battle_action;
   object front, old_target;
   int target_hp, old_hp; // victim's hp at start of action

   // If nothing to do, then do nothing.
//   if( (!action || sizeof(action) == 0) && (!target || sizeof(target) == 0) )
   if( (!action || sizeof(action) == 0) || actions_this_beat++ >= MAX_ACTIONS_PER_HEARTBEAT )
      return 0;

   // don't do stuff while held ;)
   if( held || busy )
      return 0;

   // Retrieve an action from the queue
   battle_action = action[0];

   // Check to make sure we got a valid action back...
   // battle_action[B_FNAME] is the object that performs the action,
   // so if it is 0, that implies it was invalid or dest'd.
   if( !battle_action || !battle_action[B_FNAME] ) {
      // Nope, pop the weird action and return.
      action = action[1..];
      return 2;
   }

   drop_invalid_targets();

   // Move a local target to the front of the queue.
   // There might not be one, so we check later.
   if( pointerp(target) ) {
      front = target[0];
      while( environment(target[0]) != environment(this_object()) ) {
         target = target[1..] + ({ target[0] });
         if( target[0] == front ) break;
      }
   }

   // If battle_action[B_TARGET] is -1, then it should be configured
   // to attack the "current target."
   if( battle_action[B_TARGET] == -1 ) {
      if( pointerp(target) ) {
         battle_action[B_TARGET] = target[0];

         // Actions on non-specific targets when there aren't any
         // in the room get dropped.
         if( environment(battle_action[B_TARGET]) != environment(this_object()) ) {
            action = action[1..];
           return 2;
         }
      }
      // I have no targets. Combat actions against current targets are ignored.
      else {
         action = action[1..];
         return 2;
      }
   }

   // Convert some of the shortcut object-actors to
   // object pointers.
   if( battle_action[B_FNAME] == "me" )
      battle_action[B_FNAME] = this_object();

   // Check to see if the player's readiness is enough.
   if( readiness < battle_action[B_RCOST] )
      return 0;

   // Check to see if the player's stance is right
//   debug("stance = "+(string)stance+", should be "+(string)battle_action[B_STANCE],"combat");
   if( !(stance & battle_action[B_STANCE]) )
      if (!change_stance(battle_action[B_STANCE]))
         return 0;
      else if( readiness < battle_action[B_RCOST] )
         return 0;

   // Check that target is still there...
   if( battle_action[B_TARGET] ) {
      // Ok, we're going to perform the action. Set up some basic
      // variables...
      set_this_player( this_object() );
      set_target( battle_action[B_TARGET] );
      old_target = battle_action[B_TARGET];
      target_hp = old_target->query_hp();
      old_hp = query_hp();

      action = action[1..];
      call_other(battle_action[B_FNAME], "on_action", battle_action[B_TARGET], battle_action[B_PARAM]);

      // Notify the victim/living if their hp has changed
//      if( old_target && old_target->query_autohp() && target_hp != old_target->query_hp() )
//         "/bin/hp"->force(old_target);
//      else if (this_object()->query_autohp() && old_hp != query_hp())
//         "/bin/hp"->force(this_object());
   }
   else {
      // Target vanished. Drop action.
      action = action[1..];
   }

   return 1;
}

/** Adds an action onto the end of the action queue. Equivalent
 * to set_action( sizeof(query_actions()), act ), but much
 * simpler!
 * \param act A correctly formatted action. See the man page on 'action'.
 */
int push_action( mixed *act ) {
   if( !pointerp(action) ) action = ({ });
   if( !pointerp(act) ) return 0;
   if( sizeof(act) != B_SIZE ) {
      debug("Living::push_action: Attempt to push a mis-sized battle action "+as_lpc(act),"combat");
      return 0;
   }
   // Cheater deterrant. Al [11/13/03]
   if ( sizeof(action) >= 10 ) {
      msg_object(this_object(),"~CWRNUnable to add action to queue. Be patient.~CDEF");
      return 0;
   }
   action += ({ act });
   // Turn on the heart-beat to handle the results.
   set_heart_beat(1);
   // If ready, do it now! I only do this for players, since players like
   // their reactions instant...
   if( this_object()->query_is_player() )
      return pop_action();
   else
      return 0;
}

/// Set the alignment values directly.
void set_alignment(int x, int y) {
  if( intp(x) && intp(y) && x < 2 )
    alignment[x] = (y<-1000?-1000:(y>1000?1000:y));
}

/** Allows you to set your alignment as a string. If you
 * pass in a standard alignment name (chaotic good, true neutral,
 * lawful evil, etc.) then it translates this to a numeric alignment;
 * otherwise, your alignment is set to a custom value and ignores
 * numeric fluctuations. Pass in the string "clear", or "0", or
 * the number 0 to clear a custom alignment.
 */
void set_alignment_string(string x) {
  mapping alignments;

  alignments = ([ "chaotic good":({500,-500,0}),
                  "neutral good":({500,0,0}),
                  "lawful good":({500,500,0}),
                  "chaotic neutral":({0,-500,0}),
                  "true neutral":({0,0,0}),
                  "neutral neutral":({0,0,0}),
                  "lawful neutral":({0,500,0}),
                  "chaotic evil":({-500,-500,0}),
                  "neutral evil":({-500,0,0}),
                  "lawful evil":({-500,500,0}) ]);

  if (x == "clear" || x == "0" || x == 0)
    alignment[2] = 0;
  else if( member(alignments, x) ) alignment = alignments[x];
  else
    alignment[2] = x;
}

void set_favor(string god, int amt) {
  amt = min(amt, 100);
  if (!mappingp(favor))
    favor = ([]);
  favor[FAITH_DAEMON->query_god_name(god)] = amt;
}

void set_favor_map(mapping map) { favor = map; }

/**
 * This method spends favor - without risking lightning as a result.
 *
 * NOTE: This is the amount _subtracted_ as this method is never to be used to increase
 *       an object's favor - use add_favor() for that since it gives bonuses for the
 *       actor's repentance skill.
 */
void debit_favor(string god, int amt) {
  god = FAITH_DAEMON->query_god_name(god);
  set_favor( god, query_favor(god) - amt );
}

void add_favor(string god, int amt) {
  god = FAITH_DAEMON->query_god_name(god);
  // adjust amount based on repentance skill
  // (Bizarre side note: This means that actions which
  // will badly displease a god will, in fact, please
  // them if you have a high enough repentance skill.
  // Hello?)
  amt += this_object()->skill_check("magic.faith.repentance", 20, 40, 60, 80, 100);
  if (!god || amt == 0)
    return;
  set_favor(god, query_favor(god) + amt);
  // messages to the player re their standing
  // Was "amt > 1", which is obviously wrong.
  // This meant that players were dying when
  // they did pleasing things.
  if (amt >= 0) {
    if (this_object()->skill_check("magic.faith.devotion", 50)) {
      if (favor[god] > 95)
        msg_object(this_object(),"You feel "+god+"'s presence next to you.");
     else if (favor[god] > 75)
        msg_object(this_object(),"You feel very close to "+god+".");
      else
        msg_object(this_object(),"You feel closer to "+god+".");
    }
  } else {
    if (favor[god] < -1) {
      this_object()->set_env_var("cause_of_death","has angered "+god+" one too many times.");
     // can NEVER get frotzed for a minor (1 point) infraction of favor
      if (favor[god] > -15 || amt == -1) {
        if (this_object()->skill_check("magic.faith.devotion", 50))
          msg_object(this_object(),"The ground rumbles loudly and you feel that "+god+" is displeased with you.");
        else
          msg_object(this_object(),"The ground rumbles loudly.");
      } else if (favor[god] > -30) {
        set_actor(this_object());
        this_object()->msg_local("~CWRN~Name ~verbis struck by a bolt of lightning!~CDEF");
        this_object()->take_damage( (["electricity":10]) );
        favor[god] = 0;
      } else {
        set_actor(this_object());
        this_object()->msg_local("~CWRN~Name ~verbis surrounded by a thick black cloud of smoke...~CDEF");
        this_object()->die();
        favor[god] = 0;
      }
    } else if (favor[god] < 10)
      msg_object(this_object(),"The ground rumbles briefly.");
  }
}

/// Add an amount to your readiness. See set_readiness(). Starts the heart beat.
void add_readiness(int x) { readiness += x; set_heart_beat(1); }

/// Modify your held timer by a certain amount. See set_held(). Starts the heart beat.
void add_held(int x) { set_held(held + x); }

/// Modify your busy timer by a certain amount. See set_busy(). Starts the heart beat.
void add_busy(int x) { busy += x; set_heart_beat(1); }

/// Adds an amount to your HP. This amount will be bounded to the maximum HP. Heart beat is enabled.
void add_hp(int x) {
   if ( this_object()->query_autohp() && hp < query_maxhp() && (hp + x) >= query_maxhp() )
      "/bin/hp"->force(this_object());

   hp = to_int(hp + x);
   // update: add_hp now only crops you down to max if receiving positive
   // healing, not taking damage - al [4.15.04]
   if (hp > query_maxhp() && x > 0) hp = query_maxhp();
   if (x < 0) {
      if ( this_object()->query_autohp() )
         "/bin/hp"->force(this_object());
      if ( wimpy && (100 * hp / query_maxhp() < wimpy) && target && hp > 0 )
         command("flee");
   }
   set_heart_beat( 1 );
}

/// Add an amount to your mana. Bounded by your maximum mana. Enables the heart beat.
void add_mana(int x) {
  /* This should be taken care of whenever the mana is actually being drained
   * now, so no freebie skill trains simply for losing mana.
  if (x < 0)
    practice_skill("magic.spellcraft");
  */
  if ( this_object()->query_autohp() && mana < query_maxmana() && (mana + x) >= query_maxmana() )
    "/bin/hp"->force(this_object());

  mana = to_int(mana + x);
  // only cap mana when raising, not reducing
  if (mana > query_maxmana() && x > 0)
    mana = query_maxmana();
  set_heart_beat(1);
}

/// Adds an amount to your endurance. Bounded by max endurance. Heart beat enabled.
void add_endurance(int x) {
  if ( this_object()->query_autohp() && hp < query_maxend() && (endurance + x) >= query_maxend() )
    "/bin/hp"->force(this_object());

   endurance = to_int(endurance + x);
   // major downgrade on how endurance trains now, must spend high amounts at
   // once in order to get pracs at higher levels - al [may 28, 05]
   if (x < 1)
      practice_skill_conditional("other.endurance", -25 * x, 1);
   // only cap endurance when raising, not reducing
   if (endurance > query_maxend() && x > 0)
      endurance = query_maxend();
   set_heart_beat(1);
}

/** Removes a skill entirely from the mapping.
 */
void remove_skill(string skill ///< The skill to remove.
   ) {
   m_delete( skills, skill );
}

/** Add the amount y to your alignment of type x. If x is 0, then
 * good/evil alignment is modified; if x is 1, then lawful/chaotic
 * is modified.
 */
void add_alignment(int x, int y) {
  if( !intp(x) || x > 1 ) return;
  alignment[x] += y;
  if (alignment[x] > 1000)
    alignment[x] = 1000;
  if (alignment[x] < -1000)
    alignment[x] = -1000;
}

/** Same as add_alignment( 0, n )
 */
void add_good_alignment(int n) {
   add_alignment( 0, n );
}

/** Same as add_alignment( 1, n )
 */
void add_lawful_alignment(int n) {
   add_alignment( 1, n );
}

/// Add an amount to the living's weight.
void add_weight(int x) {
  set_weight( query_weight() + x );
}

/** Add an amount to the player's food level. If the player is over 75% food capacity,
 * some of the mass actually goes to their weight in stead.
 */
void add_food(int f) {
  int w = 0;
  if (food > (75 * query_food_capacity() / 100)) {
    w = f;
    f = 9 * f / 10;
  }
  food += f;
  if (query_weight() > 0)
    add_weight(w);
  recalc();
}

/// Add an amount to the player's drink level.
void add_drink(int x) {
   drink += x;
   recalc();
}

/** Sets this living object's level. If the level desired would make
 * this object a wizard (or higher), then it is required that the
 * caller have a root uid.
 */
void set_level(int x) {
   if( x >= WIZLEVEL || x < level )
   if( getuid(previous_object()) != MASTER_UID ) {
      log_file("secure", object_name(previous_object()) +
         " (" + previous_object()->query_name() + ") tried to set_level(" + x +
         ") in " + object_name(this_object()) + " (" + query_name() + ").\n");
      return;
   }
   level = x;
}

void set_deaths(int x) {
   /* Log all attempts, successful or not */
   if( getuid(previous_object()) != MASTER_UID ) {
      log_file("secure", object_name(previous_object()) +
         " (" + previous_object()->query_name() + ") called set_deaths(" + x +
         ") in " + object_name(this_object()) + " (" + query_name() + ").\n");
   }
   /* Report all tinkering */
   if ( this_player()->query_level() >= WIZLEVEL ) {
      "/daemon/chat"->chat("death", "system",
      this_player()->query_name() + " tinkered with " +
      query_name() + "'s death count, changing it from " + deaths
      + " to " + x);
      deaths = x;
   }
}

/** Set this object's job title. Because of the way the who command
 * formats things, we truncate to 10 characters for players. Mobs can
 * technically have longer job titles, but why you'd need to... boggle.
 */
void set_job_title(string x) {
  if( (x == "Admin" || x == "admin") && getuid(previous_object()) != MASTER_UID ) {
     raise_error("Illegal set_job_title attempt from "+as_string(previous_object()));
     return 0;
   }
  if (sizeof(x) > 10 && this_object()->query_is_player())
    job_title = x[0..9];
  else
    job_title = x;
}

/// Set this living's gender to \a x. 0 = neuter, 1 = male, 2 = female.
void set_gender(int x) {
   if(x >= 0 && x <= 2)
     gender = x;
}

/// Sets the followers of this object
void set_followers( object *x ) { my_followers = x; }

/// Sets the leaders of this object
void set_leaders( object *x) { my_leaders = x; }

/// Checks the follower list of this object for valid objectp().  Removes any invalid objectp's.
void check_followers() {
   int i;

   if( !my_followers ) return;

   for( i=0;i<sizeof(my_followers);i++ )
      if( !objectp(my_followers[i]) || !my_followers[i]->query_is_living() ) {
         remove_follower( my_followers[i] );
      }
}

/// Checks the leader list of this object for valid objectp().  Removes any invalid objectp's.
void check_leaders() {
   int i;

   if( !my_leaders ) return;

   for( i=0;i<sizeof(my_leaders);i++ )
      if( !objectp(my_leaders[i]) ) {
         remove_leader( my_leaders[i] );
      }
}

/// Adds a follower to this object. Returns 1 if successful, 0 if it fails(ie if you already have that follower)
int add_follower( object x ) {
   int i;

   if( !x ) return 0;

   if( !my_followers ) {
      my_followers = ({});
   }

   check_followers();

   for( i=0;i<sizeof(my_followers);i++ )
      if( x == my_followers[i] ) return 0;

   my_followers += ({ x });
   return 1;
}

/// Adds a leader to this object.  Returns 1 if successful, 0 if it fails(ie if you already have that leader)
int add_leader( object x ) {
   int i;

   if( !x ) return 0;

   if( !my_leaders ) {
      my_leaders = ({});
   }

   check_leaders();

   for( i=0;i<sizeof(my_leaders);i++ )
      if( x == my_leaders[i] ) return 0;


   my_leaders += ({ x });
   return 1;
}

/// Removes a follower of this object
int remove_follower( object x ) {
   int i;

   for( i=0;i<sizeof(my_followers);i++ ) {
      if( x == my_followers[i] ) {
         my_followers -= ({ x });
         return 1;
      }
   }
   return 0;
}

/// Removes a leader of this object
int remove_leader( object x ) {
   int i;

   for( i=0;i<sizeof(my_leaders);i++ ) {
      if( x == my_leaders[i] ) {
         my_leaders -= ({ x });
         return 1;
      }
   }
   return 0;
}

/*************************************************** ENVIRONMENT VARIABLES */

/** @name Environment Variables
 * Variables used for tracking miscellanous settings in the player.
 */

//@{
/** Queries the value of a single environment variable. If
 * name contains an expression of the form "var[x]" where
 * x is an integer, then only the x'th element of var
 * will be returned, otherwise the entire array will be
 * returned. Indexing is 0 based.
 * \return 0 if \a name does not exist, otherwise the value.
 */
mixed query_env_var( string name ) {
   int i;

   if( !env_vars || !stringp(name) ) return 0;
   // Temp fix!
   if( stringp(env_vars["hair"]) ) env_vars["hair"] = explode( env_vars["hair"], "," );
   if( sscanf(name, "%s[%d]", name, i) == 2 ) {
      if( !env_vars[name] || !pointerp(env_vars[name]) ) return 0;
      if( i < 0 || i >= sizeof(env_vars[name]) ) return 0;
      return deep_copy(env_vars[name][i]);
   }
   return deep_copy(env_vars[name]);
}

/** \return The entire environment variable
 * mapping. If the environment variable mapping is null
 * this function will return 0. To avoid certain security
 * hazards, this function returns a copy, not the original
 * mapping.
 *
 * \todo Scrap the 'hair' patch here once inactive player
 * deletion is in place.
 */
mixed query_env_vars() {
   if( !mappingp(env_vars) ) env_vars = ([ ]);
   if( stringp(env_vars["hair"]) ) env_vars["hair"] = explode( env_vars["hair"], "," );
   return deep_copy(env_vars);
}

/** Adds \a val to the list of values for the environment
 * variable \a var. If the variable does not exist yet,
 * it will be created. Only use this function if the
 * environment variable contains an ARRAY ... attempting
 * to "add" to plain integer or string variables
 * will erase the old value and replace it with
 * a single element list containing \a val.
 */
void add_env_var( string var, mixed val ) {
   if( !env_vars ) env_vars = ([ ]);
   if( pointerp(env_vars[var]) )
      env_vars[var] += ({ val });
   else
      env_vars[var] = ({ val });
}

/** Removes an environment variable from this player's mapping of them.
 * \param var The variable to delete.
 * \return True if the variable is deleted; false if it did not exist.
 */
int remove_env_var( string var ) {
   if( member(env_vars, var) ) m_delete( env_vars, var );
   else return 0;
   return 1;
}

/** Removes \a val from the environment variable given by
 * \a var. If \a val is an integer, then the variable at
 * that position is deleted (0 based indexing); otherwise, \a var is searched
 * for a value matching \a val and that value is deleted.
 * Deleting by integer index is much more efficient.
 *
 * \return True on success; false if the parameters are invalid.
 * False is also returned if the variable contains a string
 * or integer (this function only works on array style variables).
 * Invalid index is not detected, instead it will quietly
 * do nothing.
 */
int remove_env_var_item( string var, mixed val ) {
   if( !env_vars || !pointerp(env_vars[var]) ) return 0;

   if( !intp(val) ) {
      val = member( env_vars[var], val );
      if( val < 0 ) return 0;
   }
   env_vars[var] = env_vars[var][..val-1] + env_vars[var][val+1..];
   return 1;
}

/** Set the entire environment variable's mapping
 * directly. This is really simple; useful for
 * direct variable editing.
 *
 * Any mapping or the value 0 will be accepted
 * here. Anything else is quietly ignored.
 */
void set_env_vars( mapping m ) {
   if( mappingp(m) || m == 0 )
      env_vars = m;
}

/** Sets an entire environment variable or one of the
 * values within an environment variable. To set a
 * single value within a variable, use the format
 * "varname[x]" for the \a var parameter; this
 * will set item \a x in \a varname to \a val.
 * Otherwise, the entire variable \a var will
 * be set to \a val. \a val may be of any datatype,
 * however it is recommended that you only use
 * strings, ints, and arrays of strings or ints
 * for variables.
 *
 * \return True on success, false if there was a problem.
 */
int set_env_var( string var, mixed val ) {
   int i;

   if( !env_vars ) env_vars = ([ ]);
   if( sscanf(var, "%s[%d]", var, i) == 2 ) {
      if( !pointerp(env_vars[var]) ||  i < 0 || i >= sizeof(env_vars[var]) ) return 0;
      env_vars[var][i] = val;
      return 1;
   }
   env_vars[var] = val;
   return 1;
}

//@}


/****************************************************************** COMBAT */
/** Adds \a enemy to this living's combat queue, and starts battle.
 */
void start_combat(object enemy) {
   debug("start_combat called","combat");
   if( !enemy || this_object() == enemy ) return 0;
   if( this_object()->query_dead() || enemy->query_dead() ) return 0;

   // reset readiness within valid bounds
   if (readiness >= RMAX_READINESS)
     readiness = RMAX_READINESS - 1;
   else if (readiness < RMIN_READINESS)
     readiness = RMIN_READINESS;

   if( !target ) target = ({ });
   target = target + ({ enemy });

   if( !pointerp(enemy->query_targets()) || member(enemy->query_targets(), this_object()) < 0 )
      enemy->start_combat( this_object() );
   set_heart_beat(1);
}

/// Activates your heartbeat
void cpr() {
   set_heart_beat(1);
}

/// Stops attacking everything.
stop_combat() {
   target = 0;
}

/// Announce the death to the world ;)
void death_shout() {
   object ob;
   string buf, cause;

   AUDIO_DAEMON->sfx_mud( "ugh.wav" );
   set_actor(this_object());

   // debug(as_lpc(all_inventory(this_object())));

   buf = this_object()->query_cap_name();
   if ( cause = this_object()->query_env_var("cause_of_death") ) {
      buf += " "+trim(cause);
      this_object()->set_env_var("cause_of_death",0);
   } else {
      // no specific cause set, guess
      if ( previous_object() && previous_object() != this_object() && previous_object()->query_is_living() ) {
         set_target(previous_object());
         if (previous_object()->query_is_player())
            buf += " has been slain by ~targ.";
         else
            buf += " has been slain by ~atar.";
      } else if (sizeof(target)) {
         set_target(query_target());
         if (previous_object()->query_is_player())
            buf += " has fallen in battle with ~targ.";
         else
            buf += " has fallen in battle with ~atar.";
      } else if ( present_clone("/world/game/minerock",environment(this_object())) ) {
         buf += " has drowned while mining.";
      } else if ( present_clone("/obj/hunger",this_object()) ) {
         if ( this_object()->query_drink() < this_object()->query_food() )
            buf += " has died of thirst.";
         else
            buf += " has starved to death.";
     } else
         buf += " has died.";
   }

   "/daemon/chat"->chat("death", "system", buf);
}

/** Tells this living object to die. This will create a corpse
 * in the location where this living object is.
 * \return A pointer to the corpse.
 */
object die() {
   object *oblist, ob, arena;
   int i;
   string str;

   // Patch for arena code (sidenote: I'd like to get rid of this, it oughtn't to be
   // necessary).
   arena = present_clone("/obj/arena", environment());

   // Mark myself as dead.
   dead = 1;

   // this is ahead of the immortal check so wizards can test easily
   // it passes this_object() so that items know who/what held them
   foreach ( ob : deep_inventory() ) {
      ob->on_death(this_object());
   }

   // We also send death events to the environment...
   foreach ( ob : all_inventory(environment()) ) {
      if( ob == arena )
         continue;
      ob->on_death(this_object());
   }
   if( arena ) {
     arena->on_death(this_object());
     // Arena should do this...
     dead = 0;
     return 0;
   } else {
     environment( this_object() )->on_death(this_object());
   }

   // If any of the events revived me, stop now.
   if( !dead )
      return 0;

   if( this_object()->query_env_var("immortal") ) {
      dead = 0;
      if( hp < 1 ) hp = 1;
      if( food < 1 ) food = 1;
      if( drink < 1 ) drink = 1;
      msg_object( this_object(), "You die! Wait, no you don't, you're immortal. Never mind." );
      return 0;
   }

   if( this_object()->query_is_player() ) {
      // moved announcement before stop_combat call
      death_shout();
      set_env_var("cause_of_death", "If you see this, there's a bug.  Alert a wizard for stat refund.");
      this_object()->msg_local("~Name ~verbdrop dead, ~poss body immediately collapsing into a cloud of dust. When the dust clears, ~pron ~verbhas disappeared.");
      readiness = -1000;
      stop_combat();
      set_actions( 0 );
      busy = held = 0;
      set_heart_beat( 1 );         // Certain rare cases need this, makes sure is_corpse is set.
      // volcano - ;coco2;room_60.60
     // ch temple - ;ch;oldtown;temple;temple2
     if( !query_env_var("spawn_point") ) {
         msg("~CBRTYou will now respawn here in the Temple of All Gods when you die.~CDEF");
         set_env_var("spawn_point", ";ch;oldtown;temple;temple2");
      }
      this_object()->move_player(query_env_var("spawn_point"));
      this_object()->revive();
      command( "look" );

      // count the death
      deaths++;
      str = this_object()->query_cap_name();
      switch( deaths ) {
         case 1:
            str = "Let us now observe a moment of silence in memory of "+str+"'s first death."; break;
         case 10:
            str = "Congratulations, "+str+", on number 10! You are now officially a member of the Death's Most Valued Customers Club! (And, incidentally, you're now receiving stat penalties when dying :P)"; break;
         case 25:
            str = "You win some, you lose some. Death, 25; "+str+", who cares?"; break;
         case 50:
            str = str+" is now officially over the hill..."; break;
         case 100:
            str = "Let's have a big round of applause as "+str+" breaks into three digits!"; break;
         case 200:
         case 300:
         case 400:
            str = "Well done! Bravo! "+deaths+"!"; break;
         case 500:
            str = "Egad. Enough is enough, and 500 is waay too much."; break;
         case 1000:
            str = "Hah! You thought I was going to say something witty just now, didn't you? Well 'pthbbbt' to you too! 1000 deaths my shiny metal... erm... yeah. Anyhow, that's enough of that. No more death mockery from the system for you. Honest."; break;
         default: str = 0;
      }
      if (str)
         "/daemon/chat"->chat("death", "system", str);
      // impose penalty
     if( deaths >= 10 ) {
         foreach( str : m_indices( this_object()->query_stats() ) ) {
            int old = query_real_stat(str);
            if( old > 3 )
               this_object()->set_stat(str, old-1);
         }
     }
      return 0;
   }


   // cleanup and messaging for dying mobs
   dead = 1;         // Set me dead
   busy = held = 0;
   food = query_food_capacity();
   drink = query_drink_capacity();
   set_actions( 0 );
   set_heart_beat( 1 );         // Certain rare cases need this, makes sure is_corpse is set.
   set_target( this_object() );
   stop_combat();      // Get out of combat so players don't swing when they wake
   // Move the corpse to the end of the list of objects. This *counts on*
   // the fact that moving an object to its environment puts it at the
   // front of the list.
   oblist = all_inventory( environment(this_object()) );
   for( i = sizeof(oblist) - 1; i; i-- )
      if( oblist[i] != this_object() ) oblist[i]->move( environment(this_object()), 1 );
   return this_object();
}

// struggle when grappled and something grappling in the room dies
// should really have a check for target, but I can't get it working
// -Malap 2005-07-21
/* masked to see if ai hooks are being blocked -Gpfault 2010-09-25
void on_death() {
   mapping grappling;

   debug("obj/living::on_death");

   if ("/bin/struggle"->get_first_obj(this_object())) {
      foreach(object ob:target) {
         grappling = ob->query_env_var("grappling");
         if (!mappingp(grappling)) continue;
         command("struggle", this_object());
      }
   }
}
*/

/// Recalculate various bonuses/penalties stored on the living.
void recalc() {
  object iob, ob;
  int x;

  foreach( iob : all_inventory() )
  if( iob->query_name() == "hunger_ob" ) {
     ob = iob;
     break;
  }
  if( food < 0 || drink < 0 ) {
     if( !ob ) {
        ob = clone_object( "/obj/hunger" );
        ob->move( this_object() );
     }
  }
  else if(ob)
     destruct(ob);

   stat_bonus = ([ ]);
   foreach( ob : filter(all_inventory(), (: $1->query_use_bonuses() :)) ) {
      stat_bonus["spd"] += ob->query_spd_bonus();
      stat_bonus["con"] += ob->query_con_bonus();
      stat_bonus["str"] += ob->query_str_bonus();
      stat_bonus["wil"] += ob->query_wil_bonus();
      stat_bonus["dex"] += ob->query_dex_bonus();
      stat_bonus["int"] += ob->query_int_bonus();
      stat_bonus["emp"] += ob->query_emp_bonus();
      stat_bonus["cha"] += ob->query_cha_bonus();
   }

   // People starving to death may not have hp in excess of natural max;
   // as con decreases so does max, and this is kinda weird.
   // See thread 2364 for more - Malap 2004-11-14
   if ( (food < 0 || drink < 0) && query_hp() > query_max_hp() ) {
      add_hp( -1 * (query_hp() - query_max_hp()) );
   }
}

/** This function returns the amount of water lost by the victim of a
 * particular injury.
 */
int water_loss(mapping wc) {
  int total = 0;
  string type;
  foreach (type : wc) {
    switch (type) {
      case "edged":
      case "piercing":
        total += wc[type]; break;
      case "fire":
        total += wc[type]/2; break;
      case "ice":
      case "electricity":
      case "blunt":
        total += 0; break;
      default:
        total += wc[type]/4;
    }
  }
  return total;
}

/** This function returns an integer giving the number of hit-points
 * actually lost when the damage from wc is applied. The function
 * does return 0 if passed an invalid wc mapping. It allows damage to
 * be reduced to 0 (and returns 0 if this is the case), but it is
 * impossible for damage to be negative (if you want damage absorption
 * of some kind, you need to find another way).
 *
 * It is impossible to determine whether the blow was fatal from this
 * function alone -- you should check out take_damage_percent for that.
 * Also, query_hp() + take_damage(...) might be negative following
 * this function.
 *
 * The single parameter, wc, is a mapping of the types of damage and
 * the wc of the amount of damage on each. The wc tag types are
 * arbitrary, but correspond to the ac types.
 */
int take_damage( mapping wc, int bp ) {
   int total = 0, dam_mod = 0, end_check = 0;
   mapping temp_wc = ([]);
   object inv;
   mixed foo;
   string buf = "";

   if( !mappingp(wc) ) {
     debug("Invalid wc mapping passed to take_damage().","combat");
     return 0;
   }
   temp_wc = copy(wc);

   set_heart_beat( 1 );

   total = BATTLE_DAEMON->calc_dmg_by_rules( temp_wc, bp, all_inventory(), this_object() );

   if ( total < 0 ) total = 0;

   // if they're already dead, don't kill them again
   // but we wanted to calculate damage for messaging purposes
   if( dead || this_object()->query_hp() <= 0 )
       return total;

   // endurance skill allows for buffering hp damage
   if ( total && soak && (end_check = skill_check("other.endurance", total*2, total*4, total*6)) ) {
      int end_damage = total / (end_check + 1);
      if (end_damage >= query_endurance())
         end_damage = query_endurance();
//      debug( "total: " + total + ", end_damage: " + end_damage + ", end_check: " + (end_check+1) + " soaked: " + (total * end_check / 4) );
      total -= total * end_check / 4;
      add_endurance( -end_damage );
   }

   // check for interruption
   if (pointerp(query_action(0))) {
      object actobj = query_action(0)[B_FNAME];
      mixed  actpar = query_action(0)[B_PARAM];
      string actnam = query_action_name(0);
      // DC for interruption from damage is the wc x 5, even if damage is absorbed
      // However, if damage is completely mittigated by armour, then the roll is skipped
      if (total && actobj->query_is_interruptable()) {
         int check, dc, q, total_wc = 0;
         foreach( q : m_values(wc) )
            total_wc += q;
         dc = min( 120, total_wc * 5 );
         check = skill_check("magic.concentration",dc,dc + 10);
         debug( "interrupt dc = "+dc+", check = "+check, "magic" );
         //debug("interruption check, dc = "+dc+", check = "+check);
         if (check == 1) {
            msg_object(this_object(),"~CDEFYou barely maintain your concentration.");
         } else if (!check) {
            msg_object(this_object(),"~CWRNYou lose your concentration!~CDEF");
            actobj->on_interrupt(query_action(0));
            // dequeue the action
            action = action[1..];
         }
      }
   }

   // Drop readiness a bit from getting hit.
      // OLD MATH: readiness -= total * BATTLE_DAEMON->roll( 3, 6 );
   readiness -= total * 2; // much simpler, more nicer to everyone involved
                           // also makes stunning attacks more meaningful

   // start swinging back at whoever hit you
   if( !query_in_combat() ) {
      object ob, *targs;
     foreach( ob : all_inventory(environment(this_object())) ) {
         targs = ob->query_targets();
         if( pointerp(targs) && member(targs, this_object()) != -1 ) {
            set_target(ob);
            msg_local("~CWRN~Name ~verbturn to fight ~targ.~CDEF");
            start_combat(ob);
         // reset messaging so the attack displays correctly
         set_target(this_object());
         set_actor(ob);
         }
      }
   }

   // Do the damage.
   damage_pending += total;
   // Certain types of damage cause dehydration
   add_drink ( -water_loss(temp_wc) );
   // Emit the on_hit event, if it looks like it makes sense
   if( this_player() && this_player() != this_object() )
      this_object()->on_hit( this_player(), total );

   // Hmm...should probably get rid of this and let the heartbeat mop up

   // Indeed, masked to prevent double deaths
   // -gp
/*   if( this_object()->query_hp() <= 0 )
      this_object()->die();
*/
   return total;
}

/** This function returns an integer giving the number of hit-points
 * actually lost as a PERCENTAGE of the remaining hit points.
 * This function returns 100 if the blow was a fatal one -- never higher.
 *
 * In every other sense, this function corresponds to take_damage, which
 * it calls.
 */
int take_damage_percent( mapping wc, int bp ) {
   int total;
   total = take_damage(wc, bp);

   if( query_hp() > 0 )
      return ((query_hp() + total) - query_hp()) * 100 / (query_hp() + total);
   else
      return 100;
}


/// Drops all objects that have been destructed from the target list.
void drop_invalid_targets() {
   int i;

   if( !pointerp(target) ) return;
   i = sizeof(target);
   while( 1 ) {
      i--;
      while( i >= sizeof(target) ) i--;
      if( i < 0 ) break;
      if( !target[i] || !target[i]->query_is_living() || target[i] == this_object() ) target -= ({ target[i] });
   }
   if( sizeof(target) == 0 ) target = 0;
   else
   if( environment(target[0]) != environment(this_object()) && sizeof(target) > 1 )
      target = target[1..] + ({ target[0] });
}

/** A function called every two seconds as long as this object's heart beat
 * is turned on. Used to perform recurring tasks like regenerating hit points,
 * combat, etc. If the heart beat runs through a round and does nothing,
 * it automatically turns itself off.
 */
void heart_beat() {
   string batt_msg;
   int keep_heart_beat;

   // Blueprint objects shouldn't have a heartbeat.
   if( !is_clone(this_object()) ) {
      set_heart_beat( 0 );
      return;
   }
   keep_heart_beat = 0;

   if( function_exists("ai_heart_beat", this_object()) ) {
      this_object()->ai_heart_beat();
      keep_heart_beat = 1;
   }

   if(this_object()->query_is_player())
      this_player()->set_idle_time( query_idle(this_player()) );

   actions_this_beat = 0;

   if( dead ) {
      if( !is_corpse ) {
         set_target( this_object() );
         msg_local("~Subt ~vertlet off a blood-chilling death cry! " +
            "~Npot eyes go suddenly glassy, and ~prot ~vertdrop to the ground, dead. ",0);
      }
      is_corpse = 1;
      doing = 0;
      dead++;
      if( dead > 300 ) {
         // Never dest players.
         if( this_object()->query_is_player() ) {
            this_object()->die();
            dead = 0;
         }
         else
            destruct(this_object());
         return;
      }
      keep_heart_beat = 1;
   }

   if( doing ) {
      doing_delay -= 2;
      if( doing_delay <= 0 )
         doing = 0;
      else
         keep_heart_beat = 1;
   }

   // If hp is less than maxhp and not dead, regenerate
   if( hp < query_maxhp() && !dead ) {
      heal_count++;
      if( heal_count >= query_heal_delay() ) {
         heal_count = 0;
         add_hp(1);
      }
      keep_heart_beat = 1;
   } /*else if ( hp > query_maxhp() ) {
      add_hp(-1);
      keep_heart_beat = 1;
   } */
   // If mana is less than maxmana, regenerate

   if (mana < query_maxmana()) {
      mana_heal_count++;
      if (mana_heal_count >= query_mana_heal_delay()) {
        mana_heal_count = 0;
        add_mana(1);
      }
      keep_heart_beat = 1;
   } else if ( mana > query_maxmana() ) {
      int bleed = min(-1, (query_maxmana() - mana) / 20);
      add_mana(bleed);
      keep_heart_beat = 1;
   }
   // If endurance is less than maxend, regenerate
   // ALSO: endurance does NOT regen if:
   //       1 - you are soaking damage in combat
   //       2 - you are meditating
   if ( endurance < query_maxend() &&
        query_action_name(0) != "Meditate" &&
        !( query_in_combat() && query_soak() ) ) {
      end_heal_count++;
      if (end_heal_count >= query_end_heal_delay()) {
        end_heal_count = 0;
        add_endurance(1);
      }
      keep_heart_beat = 1;
   } else if ( endurance > query_maxend() ) {
      add_endurance(-1);
      keep_heart_beat = 1;
   }
   drop_invalid_targets();

   // increase readiness if they're not held or busy
   // we will now increase readiness above MAX_READINESS if the cost of
   // the next item in the queue requires it... - al [12.18.01]
   if (readiness < readiness_required() || readiness < MAX_READINESS) {
     if (!held && !busy)
       readiness += (BATTLE_DAEMON->roll(query_stat("spd") * 3, 3)
                  + BATTLE_DAEMON->roll(query_stat("wil") * 3, 2)) *
                  query_endurance() / query_max_endurance() + 1;
     keep_heart_beat = 1;
     if( this_object()->query_is_player() && this_object()->query_autoready() > 0 )
       "/bin/ready"->force(this_object(), ""+this_object()->query_autoready());
   }
   /***
    Note: this section seems to be inaccurate now.

    Readiness does not increase by a set amount each hb, growth varries.
    Maximum growth will be (speed*8 + will*12)
    Average growth will be (speed*4 + will*6)
    Mimimum growth will be (speed*2 + will*2)
    With 10 speed and 10 will, growth = [40..200] (100 average)
         10   "       15  "      "      [50..260] (130 average)
         15   "       10  "      "      [50..240] (120 average)
         20   "       20  "      "      [80..400] (200 average)
   ***/

   // decrement hold and busy times
   if (held > 0) {
     add_held(-1);
   }
   if (!held && (busy > 0)) { // busy does not decrease if held :)
     busy--;
     keep_heart_beat = 1;
   }

   // Perform battle actions as necessary. If no action is on the queue,
   // give the form an opportunity to put one there first.
   if( (!action || sizeof(action) < 1) && readiness >= 1000 && query_in_combat() ) {
      this_object()->on_combat_tick( query_form_object() );
      query_form_object()->default_action(this_object());
   }

   while( pop_action() == 1 ) {
      keep_heart_beat = 1;
   }

   // inflict all damage that has been building up this hb
   if (damage_pending) {
     add_hp( -damage_pending );
     damage_pending = 0;
//     if( this_object()->query_autohp() )
//       "/bin/hp"->force(this_object());
   }
   // check if they're dead
   if( this_object()->query_hp() <= 0 && !this_object()->query_dead()) {
      this_object()->die();
   }

   // keep heartbeat going if fighting
   if( query_in_combat() ) {
      keep_heart_beat = 1;
      if( this_object()->query_is_player() ) {
         string bgm = this_object()->query_env_var("battle_music");
        // play battle music
        if( !bgm ) {
            bgm = AUDIO_DAEMON->query_battle_music();
         debug("battle music = "+bgm,"audio");
            this_object()->set_env_var("battle_music", bgm);
         }
         AUDIO_DAEMON->bgm(this_object(), bgm);
      }
   } else if( this_object()->query_env_var("battle_music") ) {
      debug("purging "+this_object()->query_name()+"'s battle music", "audio");
      this_object()->set_env_var("battle_music", 0);
      AUDIO_DAEMON->start_the_music( this_object() );
   }
   // drop stuff if they're over max carry capacity
   if( query_held_weight() > query_weight_capacity() ) {
      object *drop_me = ({ });
      object *inv = filter(all_inventory(this_object()), (: $1->query_droppable() :));
     int goal = query_held_weight() - query_weight_capacity(), t = 0;
      this_object()->msg_local("~CACT~Name ~verbfumble ~poss inventory.~CDEF");
     inv = sort_array(inv, (: $1->query_weight() < $2->query_weight() :) );
     // This doesn't work in some cases :P
     //    drop_me = limit_weight( inv, max(to_int(goal * 1.25),450) );
     // New math works more reliably but _still_ isn't perfect
     while( sizeof(inv) && goal > 0 ) {
         object *tmp = limit_weight( ({ inv[0] }), goal );
       if( sizeof(tmp) ) {
            drop_me += tmp;
            goal -= tmp[0]->query_weight();
       } else {
            // unconditional shedding of held or worn stuff
            // bandaid until we take into account clothing layers
            inv[0]->set_held(0);
            inv[0]->set_worn(0);
            drop_me += ({ inv[0] });
         break;
       }
         inv = inv[1..];
      }
      set_actor(this_object());
     // By turning drop into a queued action, we're able to specify what
     // gets dropped with much more reliability than we could when trying
     // to pass a stupid string...
     catch( "/bin/drop"->on_action(0, drop_me) );
   }

   // clean out cause of death - might be a bad idea here, shrug
   // - or it might just be unnecessary, either way, we leave this in for
   // now... - al (8/17/04)
   set_env_var("cause_of_death",0);

   set_heart_beat(keep_heart_beat);
}

/// Recalculates bonuses, checks the weight limit.
void on_insert( object ob ) {
   if( !environment() ) return;
   ob->set_held( 0 );
/*
   if( query_held_weight() > query_weight_capacity() && ob->query_weight() > 0 ) {
      set_listener( this_object() );
      set_target( ob );
      msg_object( this_object(), "~CWRNYou're overburdened, so you drop ~targ.~CDEF" );
      ob->move( environment() );
      ob->set_coord( query_coord() );
   }
*/
   recalc();
}

/// Recalculates bonuses, etc.
void on_remove( object ob ) {
   if( !objectp(ob) ) return;
   call_out( "recalc", 0 );
   ob->set_held( 0 );
}

void practice_stat( string stat, int amt ) {
   "/daemon/counter"->add_count( stat + "_count" );
   add_stat_use( stat, 1 );
}

/** Call this function whenever you want to practice a skill.
 * Generally called when the skill is used.
 *
 * \param skill Name of the skill to be practiced
 * \param amt (Optional) Amount of points to practice by. Default is 1.
 */
varargs void practice_skill( string skill, int amt ) {
   string istat, *used_stats;

   debug( "practice_skill "+skill+" "+amt, "prac" );

   if( skill == 0 || skill == "" ) {
      debug( as_string(previous_object()) + " attempted to practice null skill.", "combat" );
      return;
   }

   if( !SK_DAEMON->query_skill_exists(skill) ) {
      debug( as_string(previous_object()) + " attempted to practice nonexistent skill " + as_lpc(skill), "combat" );
      return;
   }

   if( !amt )
      amt = 1;

   used_stats = SK_DAEMON->query_skill_stats( skill );
   if( used_stats )
   foreach( istat: used_stats )
      practice_stat( istat, amt );

   // if they've already maxxed out the skill, don't bother
   if( query_skill_level(skill) == 100 )
     return;

   if( member(skills,skill) ) {
      debug( "practicing "+skill+" for "+amt, "prac" );
      // removed cap on practices per call ;) - al (4/23/04)
      // amt = (int) (min(amt, this_object()->query_intelligence() / 4));
      add_skill( skill, amt );
   }
}

/** This practices a skill - but only if the player's skill is below
 * the cutoff point specified.
 * \return 1 if the skill was practiced, 0 if not
 */
int practice_skill_conditional( string skill, int cutoff, int amt ) {
   if( query_skill_level(skill) < cutoff ) {
      practice_skill(skill, amt);
     return 1;
   } else {
     return 0;
   }
}

/** This performs a simple skill roll, and returns the result.
 * It is the core math behind skill_check. This includes a call
 * to random(), so expect different numbers back each time
 * (hence it's a get_, not a query_). It will return -11 if the
 * skill doesn't exist (-10 is the worst possible normal roll).
 */
int get_skill_roll( string skill ) {
   if (!SK_DAEMON->query_skill_exists(skill))
      return -11;

   return query_modified_skill( skill ) + random(50) - 10;
}

int get_save_roll( string stat ) {
   return query_stat(stat) * 5 + random(50) - 10;
}

int save_check( string stat, int dc ) {
   int roll = get_save_roll(stat);
   practice_stat(stat, 1);
   return roll >= dc;
}

/** skill_check is called with one or more difficulty classes.
 * Calling it means that the player is attempting to perform
 * an action using the skill \a skill, and that the action has
 * one or more levels of success. For example:
 *
 * skill_check( "name.of.skill", 10 ) -- returns 0 or 1.
 * skill_check( "name.of.skill", 10, 20 ) -- returns 0-2.
 * skill_check( "name.of.skill", 10, 20, 30 ) -- returns 0-3.
 *
 * It returns -1 if you don't pass in a valid skill name. A
 * 0 means that the skill check failed.
 *
 * Calling this function practices the skill in question.
 * The worse you are at a skill, the more practice it gives you
 * -- once you hit the highest DC 100% of the time, it no
 * longer gives you any practice at all.
 *
 * If you prefix the skill name with "nopractice:", then the
 * practicing of skills will NOT be performed. For example,
 * skill_check( "nopractice:knowledge.carpentry", 10, 40 );
 */
varargs int skill_check( string skill, varargs int *dc ) {
   int skill_level;
   int success, do_practice = 1;

   if( !stringp(skill) ) return -1;

   if( sscanf(skill, "nopractice:%s", skill) == 1 )
      do_practice = 0;

   // Please do not leave spamminess after you're done testing. Malap 2004-09-26
   //debug( "Skill check called." );
   if (!SK_DAEMON->query_skill_exists(skill))
      return -1;

   if( !dc || !sizeof(dc) ) {
      debug("You have to specify DC's when calling skill_check now. You called from " + as_string(previous_object()) + ", please fix it.");
      return 0;
   }

   skill_level = get_skill_roll( skill );
   success = 0;
   while( success < sizeof(dc) && skill_level >= dc[success] ) success++;

   debug( "skill_check "+skill+" "+skill_level+" "+success+" "+do_practice+" "+sizeof(dc), "prac" );

   // curious... this was <, which means that any skill check that only possesses
   // one dc is never possible to train... and it is impossible to get practice
   // when you pass the whole check. This is not the desired behavior.
   //
   // Correct behavior is to not give practices when the skill is so high that
   // the roll could not possibly have failed on the highest dc check.
   // - al [may 29 '05]
   if( (query_skill_level(skill) - 40) >= dc[<1] )
      do_practice = 0;

   if( do_practice && success <= sizeof(dc) )
      practice_skill( skill, sizeof(dc) - success );
   return success;
}

/** Does the same thing as skill_check, except that it can 'remember' the results
 * of a previous skill check so as to make an action repeat the same way every time
 * for a certain period of time. For example, it shouldn't be possible to get more
 * info by searching or considering repeatedly, unless a certain length of time
 * has expired. The 'tag' should be something that uniquely identifies this particular
 * attempt, so that search attempts will only stick in one room, and consider attempts
 * will only stick to one creature type, etc. The 'time' is how long until the cached
 * check should expire.
 */
varargs int cached_skill_check( int time, string tag, string skill, varargs int *dc ) {
   tag = tag + "\n" + skill;
   if( cached_skill_time[tag] + time > time() )
      return cached_skill_value[tag];

   cached_skill_time[tag] = time();
   return cached_skill_value[tag] = apply( #'skill_check, skill, dc );
}

/// For debugging.
mapping query_cached_skill_time() {
   return cached_skill_time;
}

/// For debugging.
mapping query_cached_skill_value() {
   return cached_skill_value;
}

/// For debugging.
void flush_cached_skills() {
   cached_skill_time = ([ ]);
   cached_skill_value = ([ ]);
}

/// Like skill_check, only for stats
varargs int stat_check(string stat, int one, int two, int three) {
   int prac, check, bonus;
   string str;

   bonus = this_object()->query_stat(stat);
   bonus = (int)(bonus * SK_DAEMON->query_stat_skill_bonus( ({bonus}) ));
   check = bonus + random(20);

   if (three && check >= three)
      prac = 3;
   else if (two && check >= two)
      prac = 2;
   else if (one)
      prac = (check >= one);
   else
      return check;
   if (prac)
      practice_stat(stat, prac);
   return prac;
}

/// Brings this object back to life, if dead, and starts the heart beat for healing.
object revive() {
   dead = is_corpse = 0;
   if( food < 50 ) food = 50;
   if( drink < 50 ) drink = 50;
   if( hp < 10 ) hp = 10;
   if( readiness < 500 ) readiness = 500;
   set_heart_beat(1);

   // get rid of bleeders

   debug("Revive called: " + query_name() );

   foreach( object ob: all_inventory(this_object()) ) {
      if (ob->query_is_bleeder()) {
         destruct(ob);
      }
   }


   return this_object();
}

/*********************************************************** INFO DISPLAYS */

/** Sends a message to all users, with this object as the speaker.
 * This object does not hear the message.
 */
say_all(string words, int indent) {
  int i;
  object ob;

  ob = users();

  if(ob) {
    for(i=0;i<sizeof(ob);i++) {
      if(ob[i] != this_player() && is_living(ob[i])) msg_object(ob[i], words, indent);
    }
  }
}

/** Sends a message to all users in this room, with this object speaking.
 * This object does not hear the message.
 */
varargs void say_local(string words, int indent) {
  object ob;

  if(!environment(this_object())) return;
  set_actor(this_object());
  if( this_object()->query_invis() ) {
     words = "<invis> " + words;
     foreach( ob : all_inventory(environment(this_object())) ) {
       if( ob == this_object() || !ob->query_is_living() || ob->query_level() < WIZLEVEL ) continue;
       set_listener(ob);
       msg_object(ob, words, indent, this_object());
     }
  }
  else {
     foreach( ob : all_inventory(environment(this_object())) ) {
       if( !ob->query_is_living() || ob == this_object() ) continue;
       set_listener(ob);
       msg_object(ob, words, indent, this_object());
     }
  }
}

/** Send a message to all users in this room, including this object,
 * with this object as the speaker. Verbs are conjugated according
 * to codes, etc.
 */
varargs void msg_local(string words, int indent) {
  object ob;

  if(!environment(this_object())) return;
  set_actor(this_object());
  if( this_object()->query_invis() ) {
     words = "<invis> " + words;
     foreach( ob : all_inventory(environment(this_object())) ) {
       if( !ob->query_is_living() || ob->query_level() < WIZLEVEL ) continue;
       set_listener(ob);
       msg_object(ob, words, indent, this_object());
     }
  }
  else {
     foreach( ob : all_inventory(environment(this_object())) ) {
       if( !ob->query_is_living() ) continue;
       set_listener(ob);
       msg_object(ob, words, indent, this_object());
     }
  }
}
/** Send a message to all users in this room, except this object
 * and the target.
 */
varargs void msg_observers(string words, int indent) {
  object ob;

  if(!environment(this_object())) return;
  set_actor(this_object());
  if( this_object()->query_invis() ) {
     words = "<invis> " + words;
     foreach( ob : all_inventory(environment(this_object())) - ({ query_target(), this_player() }) ) {
       if( !ob->query_is_living() || ob->query_level() < WIZLEVEL ) continue;
       set_listener(ob);
       msg_object(ob, words, indent, this_object());
     }
  }
  else {
     foreach( ob : all_inventory(environment(this_object())) - ({ query_target(), this_player() }) ) {
       if( !ob->query_is_living() ) continue;
       set_listener(ob);
       msg_object(ob, words, indent, this_object());
     }
  }
}

/** Move this player to \a dest using movement type \a movetype.
 * The variable \a dest may be an object or string; if a string, it
 * must respond to find_room. Returns true on success. Also now
 * contains code to implement 'follow'.
 */
varargs int move_player(mixed dest, string moveverb, string direction) {
   object ob, env_ob, tmp_ob, iob;
   string depart, arrive, inv_dir;
   int exit_follow_flag;
   mapping curr_mobj;

   if(stringp(dest)) {
      ob = find_room(dest, environment(this_object()));
      if(!ob) {
         msg("Sorry, ran into a bug. Can't figure out where I'm supposed to go.");
         return 0;
      }
   } else ob = dest;

   inv_dir = INVERSE_EXITS[direction];
   if( member((["in", "out", "enter", "exit"]), inv_dir) ) {
      inv_dir = 0;
   }
   if( member((["north", "south", "west", "east", "northwest",
      "northeast", "southwest", "southeast"]), inv_dir) ) inv_dir = "the " + inv_dir;

   if( !stringp(moveverb) ) {
      depart = "~Name ~verbleave.";
      arrive = "~Name ~verbarrive.";
   }
   else switch( moveverb ) {
   case "stairs":
      if( direction == "down" )
         depart = "~Name ~verbgo downstairs.";
      else
         depart = "~Name ~verbgo upstairs.";
      arrive = "~Name ~verbarrive" + (inv_dir?" from "+inv_dir+"stairs":"") + ".";
      break;
   case "door":
   case "gate":
      depart = "~Name ~verbleave" + " through " + direction + ".";
      arrive = "~Name ~verbarrive" + (inv_dir?" from "+inv_dir:"") + ".";
      break;
   case "teleport":
      depart = "~Name ~verbdisappear in a puff of smoke.";
      arrive = "~Name ~verbappear in a puff of smoke.";
      break;
   case "hole":
      if( direction == "up" ) {
         depart = "~Name ~verbclimb up through the hole.";
         arrive = "~Name ~verbclimb up from below.";
      }
      else if( direction == "down" ) {
         depart = "~Name ~verbclimb down into the hole.";
         arrive = "~Name ~verbclimb down from above.";
      }
      else {
         depart = "~Name ~verbleave through the hole.";
         arrive = "~Name ~verbarrive.";
      }
      break;
   default:
      if( direction ) {
         depart = "~Name ~verb" + moveverb + " " + direction + ".";
         arrive = "~Name ~verbarrive" + (inv_dir?" from "+inv_dir:"") + ".";
      }
      else {
         depart = "~Name ~verb" + moveverb + ".";
         arrive = "~Name ~verbarrive." + (inv_dir?" from "+inv_dir:"") + ".";
      }
      break;
   }
   this_object()->msg_local( "~CACT" + depart + "~CDEF" );
   env_ob = environment();
   if( env_ob )
   foreach( iob: all_inventory(environment()) )
      iob->on_depart(this_object(), moveverb, direction, ob);
   this_object()->move(ob);
   this_object()->on_arrive(env_ob);
   this_object()->validate_position();
   this_object()->say_local( "~CACT" + arrive + "~CDEF" );

   refresh_room( env_ob, this_object() );
   refresh_room( environment(), this_object() );
   refresh_full( this_object() );

   if( my_followers ) {
      check_followers(); // Object validation...

      foreach( iob : my_followers ) {
         // Checking to see if the follower is present in the room,
         // then if so we move them as well by
         // using the 'walk' function.
         if( objectp(iob) && present(iob, env_ob) ) {
            foreach( curr_mobj : all_inventory(env_ob) ) {
               tmp_ob = curr_mobj->get_destination();
               if( tmp_ob && tmp_ob == ob ) {
                  iob->goto( curr_mobj );
                  break;
               }
            }
         }
      }
   }

   // Make invisible wizards invisible to mobs too
   if (this_object()->query_invis()) {
      foreach( iob: all_inventory(environment()) ) {
         this_object()->on_sight(iob, env_ob);
      }
   } else {
      foreach( iob: all_inventory(environment()) ) {
         iob->on_sight(this_object(), env_ob);
         this_object()->on_sight(iob, env_ob);
      }
   }
   return 1;
}

void on_walk( object target, int start, int end ) {
   object ob;
   if( target != this_object() || !pointerp(query_followers()) ) return;
   check_followers();
   foreach( ob : query_followers() ) {
      // only walk people in the same room
      if (environment(ob) == environment(this_object()))
         ob->walkto(this_object());
   }
}

void delayed_walk( mixed exit_id ) {
   if( objectp(exit_id) && exit_id->query_is_exit() )
      push_action( ({ 0, load_object("/bin/go"), exit_id, ST_STANDING, "" }) );

   else if( stringp(exit_id) )
      push_action( ({ 0, load_object("/bin/go"), this_player(), ST_STANDING, exit_id }) );
}

void goto( mixed exit_id ) {
   call_out( "delayed_walk", 0, exit_id );
}

void walkto( mixed dest ) {
   object oldtp = this_player();

   if( query_adjacent(dest) ) return;
   set_this_player( this_object() );
   if( objectp(dest) ) {
      mapping shape;
      if( shape = dest->query_border() ) {
         WALK_DAEMON->init_walk_action(
            shape, dest->query_coord(),
            ({ 0, this_object(), dest, ST_STANDING, 0 })
         );
      }
      else {
         WALK_DAEMON->init_walk_action(
            WALK_DAEMON->shape_adjacent(),
            dest->query_coord(),
            ({ 0, this_object(), dest, ST_STANDING, 0 })
         );
      }
   }
   pop_action();
   set_this_player( oldtp );
}
