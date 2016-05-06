#include <living.h>
#include <const.h>
#include <keyboard.h>
#include <textdraw.h>
#include <alias.h>
#include <interface.h>
#include <object.h>
#include <ai.h>
#include <telnet.h>

#ifndef IRCD
#define IRCD "/secure/sockets/ircd"
#endif

/* Note about interface.h: If you've used the add_ and drop_ interface commands,
 * you've probably wondered a little bit about how I managed them. Well,
 * wonder no more. This inherited object (obj/interface) allows them to work.
 * It contains a stack based system, using input_to, that handles the
 * interfaces. Anything you can do with input_to, you can do with an interface,
 * so access to input_to has been severely restricted because it messes up
 * the interfaces.
 */

/**
 * Defines only those properties which apply to players, and not monsters.
 * The sets and queries are the same style as in most objects, although
 * some of them have special purposes.
 *
 * All players have a set of environment variables. These variables are
 * designed to let you store information in the player which will be
 * remembered even after they log off without having to modify player.c.
 *
 * The Player Creation functions are used primarily when a character is created. The
 * new_password() series of functions are also used when the player
 * changes his password. The enter_world() function is called immediately
 * after logging in, and sets up the player object in the world. To
 * understand how logging on works, you should review this function carefully.
 */
/// Defines the player object

/**************************************************** FORWARD DECLARATIONS */

// Old style LPC doesn't need them, but the newer style does.

varargs void display_file(string name, int line);
static varargs void new_password1(string str);
varargs void enter_world( int reentry );
static int find_catcher();
void save_self();
void write_prompt();
static void new_player_gender(string str);

/*************************************************************** VARIABLES */

string password; /**< The player's password, stored in an encrypted form. */
/// The player's email address, as they entered it.
string email;
/// True if the player wants their email address kept secret, false if finger can display it.
int hide_email;
/// Time when player first entered the game, in seconds since epoch time
int creation_date;

/// 0 is normal, 1 is totally invis, 2 is invis only in rooms (show up on channels, wholist, quit, etc)
int invis;
/// Time when player became a wizard, in seconds since epoch time
int wiz_creation_date;

string prompt, ///< The player's CURRENT prompt -- what is showing now.
       default_prompt, ///< The player's DEFAULT prompt -- what it reverts to after an interface.
       format_prompt; ///< The player's prompt with ANSI, etc. already added in.
string path; ///< The "current directory" for wizards, not used by players.
string title, pretitle; ///< The player's title follows their name, and pretitle precedes it. See query_title().
nosave string afk;
/**< False (0) if this player is not AFK, Set to a string giving an AFK message if they are.
 * \todo Implement afk commands and add set_afk, query_afk to the player.
 */
string *invenlist; ///< Used by saveload routines -- the player's inventory.
nosave string *command_history; ///< A list of recently typed commands, used by %% (the "quicktyper").
nosave int idle_time; ///< Time idle; set in heart_beat and used by modify_command()

nosave string client_code, client_ver; ///< Contains mud-client information for multimedia, etc.

/// For wizards who're on probation
int warnings;

int last_login, ///< Timestamp for when this player's last login was
    last_logout, ///< Timestamp for when this player last logged out
    age; ///< This player's total age on the MUD, in seconds
string last_ip; ///< Last address player logged in from
/** TRUE if you've already been interrupted since you last issued a command.
 * Set to FALSE each time modify_command is called.
 */
nosave int command_mark;
nosave int icommand; ///< Internal flag used for command history

/// This variable tracks where you quit, so you start there. It's a room reference.
string current_location;
/// A newer system for tracking player location. Will phase out current_location.
symbol location;

int term_width, ///< This player's terminal width (as they set it, defaults to 79)
    term_height; ///< This player's terminal height (their setting, defaults to 24)

/// A mapping of user-defined aliases
mapping aliases;
// std_aliases are built in aliases that most people have forgotten exist,
// though they get annoyed if you don't put them in.
nosave mapping std_aliases;
/** A list of boneheads that I wish to block tells from. Functionality
 * might eventually be added to block chats from these people as well, but
 * we shall see...
 */
mapping ignore;

/** A mapping of notes-to-self by the player (very similar to the global
 * todo list)
 */
mapping memo;

/// ed flags.
int flags;

/// The last MAX_SAY_HISTORY says directed at this player
nosave mixed *say_history;
/// The last MAX_TELL_HISTORY tells directed at this player
//nosave string *tell_history;
nosave mixed *tell_history;

/// A state variable used for input capture
nosave int _in_receive;

/// A state variable used at object creation time.
int creation_flag;

/// Used while setting a password.
nosave string newpass;

/// Whether someone has been toaded -- they effectively cease to exist
int toad;

int search_alias(string params);

DISTANT( "a bland human" )
SPECIFIC( "the bland human" )
PLURAL( "bland human" )
LOOK( "~Name ~verbis $(ADJ) $(GENDERNOUN) with $(SKIN), $(HAIR), and $(EYES). ~Pron ~verbis ?(ISCLOTHED:wearing $(WORN)):(nekkid!), and carrying $(HELD).\n\n~Pron ~verbis $(HP)." )

void reset_look()
{
   set_look("~Name ~verbis $(ADJ) $(GENDERNOUN) with $(SKIN), $(HAIR), and $(EYES). ~Pron ~verbis ?(ISCLOTHED:wearing $(WORN)):(nekkid!), and carrying $(HELD).\n\n~Pron ~verbis $(HP).");
}

/***************************************************************** QUERIES */

/** @name Queries
 * For more information on a query, see the equivalent set function.
 */
//@{
/// \return True if this wizard is invisible
int query_invis() {
   if (level < WIZLEVEL)
      return 0;
   else
      return invis;
}

/// \return True if the player is using the full-screen editor
object query_editing() {
   return present_clone("/obj/fulledit",this_object());
}
/// \return True if the player is in the mailer
object query_mailing() {
   return present_clone("/obj/mailmsg",this_object());
}
/// \return True if the player is going through character creation
object query_creating() {
   return present_clone("/obj/character_creation",this_object());
}
/** Queries whether this object is a player.
 * \warning query_is_player() is for convenience, and is obviously forgeable.
 * Do not use it for security purposes.
 * \return True, indicating that this object is a player.
 */
int query_is_player() { return 1; }
/// \return This player's terminal width (in columns)
int query_term_width() { return term_width; }
/// \return This player's terminal height (in rows)
int query_term_height() { return term_height; }
/** Shadowing the player object (i.e. allowing an object to
 * replace some of its functions with alternative definitions)
 * is a gaping security hole. This function always returns true,
 * instructing the driver to forbid shadowing of this object.
 */
int query_prevent_shadow() { return 1; }
/// \return This player's email address.
string query_email() { return email; }
/// \return True if this player's email address is hidden.
int query_hide_email() { return hide_email; }
/// \return True, if this player can receive ANSI codes (for colors, cursor movement, etc.); false otherwise.
int query_ansi() { return query_env_var("Tansi"); }
/// \return The current working directory for this player. Only applies to wizards.
string query_path() { if( !path ) path = "/home/" + name; return path; }
/// \return An integer giving the date/time when this player last logged in.
int query_last_login() { return last_login; }
int query_last_logout() { return last_logout; }
int query_age() { return age; }
string query_last_ip() { return last_ip; }

int query_brief() { return this_object()->query_env_var("Tbrief"); }

string query_client_ver() { return client_ver; }
string query_client_code() { return client_code; }

void update_client_map() {
   object ob;
   if( client_code == "wclient" ) {
      ob = present( "_wccom", this_object() );
      if( ob )
         ob->print_map();
   }
}

/// Used by the Walraven client.
void set_client_code( string s ) { client_code = s; }

/// \return If name is specified, whether they are being ignored, otherwise, returns the entire ignore list.
varargs mixed query_ignore( string name ) {
   if (!ignore) return 0;
   if (name) return member(ignore,name);
   return ignore;
}

string query_afk() { return afk; }

status query_autohp() { return this_object()->query_env_var("Tautohp"); }
status query_autoready() { return this_object()->query_env_var("Tautoready"); }

string query_title() {
  if (title==0)
    return "";
  else
    return title;
}

string query_pretitle() {
  if (pretitle==0)
    return "";
  else
    return pretitle;
}

int query_warnings() {
  return warnings;
}

/** \return True (1) if the character is a testchar. This is tested by checking
 * if the "type" variable is set to "testchar\\n".
 */
int query_testchar() {
  if (::query_type() == "testchar\n")
    return 1;
  else
    return 0;
}

/// Returns creation_flag
int query_creation_flag() { return creation_flag; }

/// Returns toad flag
int query_toad() { return toad; }

/** An archaic function occasionally used to differentiate between
 * the player's current name and his real name. This MUDlib
 * prefers to assume that the player's current name is their
 * real name, and that other variations on the name are stored
 * in alternative variables. This function returns the current name.
 */
string query_real_name() {
   return name;
}

/// \return The player's say history array
string *query_say_history() {
   int x;
   if (this_player() && this_player() != this_object() && this_player()->query_level() < ADMLEVEL) {
      return ({ "Sorry, this person's say history is private." });
   }

   if ( !say_history )
      say_history = ({});
   return say_history;
}

/// \return The player's tell history array
string *query_tell_history() {
   int x;
   if (this_player() && this_player() != this_object() && this_player()->query_level() < ADMLEVEL) {
      return ({ "Sorry, this person's tell history is private." });
   }
   //if( previous_object() != this_object() && getuid(previous_object()) != "root" && as_string(previous_object()) != "bin/tellhist")
   //   return ({ "Sorry, this person's tell history is private." });

   if ( !tell_history )
      tell_history = ({});
   return tell_history;
}

/// This function hands the memo mapping over to a verb for tweaking
mapping query_memo() {
   return memo;
}

/// \return A modified distant string that handles recognition.
string query_distant() {
   return "?(RECO("+query_name()+"):"+query_cap_name()+"):("+::query_distant()+")";
}

int query_scenery() {
   return query_invis();
}

/// \return A modified specific string that handles recognition.
string query_specific() {
//   debug("living gives: " + as_lpc(::query_specific()) );
//   return "the player";
   return "?(RECO("+query_name()+"):"+query_cap_name()+"):("+::query_specific()+")";
}

/// \return Seconds since epoch time that the player first entered the game.
int query_creation_date() {
   return creation_date;
}

/// \return Seconds since epoch time that the player became a wizard. 0 if not a wizard.
int query_wiz_creation_date() {
   return wiz_creation_date;
}

//@}

/** Paint the player. Do it in white if they are this_player().
 */
void on_paint(object painter) {
   int char;
   // Get to see the first letter of their name if known
   if (this_player() && this_object() != this_player() && this_player()->query_recognize(name))
      char = name[0] - 32;
   // but for unknown people and themself, it's just @
   else
      char = '@';

   if( query_invis() ) {
      if( this_player() == this_object() )
         painter->paint( query_x(), query_y(), char, 14, LAYER_LIVING);
      else if( this_player()->query_level() >= WIZLEVEL )
         painter->paint( query_x(), query_y(), char, 8, LAYER_LIVING);
   }
   else {
      painter->paint(   query_x(),
         query_y(),
         char,
         this_object()==this_player()?15:12, LAYER_LIVING);
   }
}

/**************************************************************** ED SETUP */

// These three functions are used to store the ed setup for a player.

/** @name ED Setup
 */
//@{
/** Sets an ed flag. See also ed.c. */
void set_flag(int i ///< A number 0-31 giving the flag to be set.
   ) {
   flags = flags | (1 << i);
}

/** Clears an ed flag. */
void clear_flag(int i ///< A number 0-31 giving the flag to be cleared.
   ) {
   flags = flags & (0xFFFF - (1 << i));
}

/** \return 0 if the flag given by i is cleared, non-zero if it is not. */
int test_flag(int i ///< The flag number 0-31 which is to be tested.
   ) {
   return flags & (1 << i);
}
//@}

/******************************************************************** SETS */

/** @name Sets */
//@{
/// Sets this wizard as visible (0) or invisible (1)
void set_invis(int x) {
   if (query_level() < WIZLEVEL)
      return;
   if (x)
      invis = x;
   else
      invis = 0;
   "/bin/who"->wholist_dump();
}
/** Sets the time, in seconds, when the player last logged in. This
 * is measured in seconds since January 1, 1970. You may
 * get the current time according to the system clock by using
 * the time() efun. This value is used to calculate an updated value for the
 * player's age when they log out, and is also used by
 * the finger command to have age appear to increase while the
 * player is logged in.
 *
 * See also set_last_logout().
 */
void set_last_login(int when) { last_login = when; }
/** Sets the time that the player last logged out, similar
 * to set_last_login(). Used to calculate an updated
 * value for the player's age when they log out.
 */
void set_last_logout(int when) { last_logout = when; }
/** Sets a player's age, measured in seconds.
 */
void set_age(int x) { age = x; }
/** Sets the player's title, which is appended to the name in
 * some circumstances (notably the finger command).
 *
 * See set_pretitle().
 */
void set_title(string x) { title = x; }
/** Sets a prefix (or pretitle) for the player's name, usually
 * printed out together with the title. It can be such things
 * as "Mrs.", "Sir", "H.R.H." or what have you.
 *
 * See set_title().
 */
void set_pretitle(string x) { pretitle = x; }
/** The brief flag may be set to either true (1) or false (0).
 * If true, the player will only see room titles, not full
 * room descriptions, as they move from room to room.
 */
void set_brief(int x) { this_object()->set_env_var("Tbrief", x); }

/** Sets the character's afk message. 0 means they have returned.
 */
void set_afk(string msg) { afk = msg; }

/** Sets whether this character is a test character. Wizards sometimes
 * create test characters which they use to test their areas. In order
 * to mark these as different from normal players, use the "testchar"
 * verb, which calls this function. Passing it 1 makes the
 * character a test character, passing it 0 turns this off.
 * \warning Modifies the type field inherited from desc.
 */
void set_testchar(int x) {
  if (x == 1)
    set_type( "testchar\n" );
  else set_type( x );
}

/// Sets creation flag
void set_creation_flag(int i) { creation_flag = i; }

/// Sets and logs the toad flag
set_toad(int t) {
   // I have written and commented out this check because I know someone
   // will think to add it, but it's possible an admin account might be hacked - Malap
   //if (this_object()->query_level() >= ADMLEVEL) return;

   log_file("toad", this_player()->query_name() + " set toad flag on " + this_object()->query_name() + " to " + t + "\n");
   toad = t;
}

/** Sets the line-wrap width for this player. This function is
 * is used by the termsize verb to change wrapping width. The
 * minimum value is 20; if you set it any lower, it will
 * set it to 20.
 *
 * See also set_term_height().
 */
void set_term_width( int x ) {
   if( x < 20 ) term_width = 20;
   else term_width = x;
}

/** Sets the terminal height for this player. This is mainly used for
 * special purpose file display, such as the more command, or the full
 * screen text editor. The minimum value is 3, although some code
 * (such as the editor) is unlikely to work properly unless it is at
 * least 10.
 *
 * See also set_term_width().
 */
void set_term_height( int x ) {
   if( x < 3 ) term_height = 3;
   else term_height = x;
}

/** Turns ANSI codes on or off. Pass true to turn ANSI codes on,
 * false to turn them off. ANSI codes are special character sequences
 * which are not printed literally. When a terminal receives an
 * ANSI code it interprets it as an instruction. ANSI
 * codes are used for colors, moving the cursor, clearing the
 * terminal window, and so on.
 */
void set_ansi(int x) { set_env_var("Tansi", x); }
/** Sets the player's email address.
 */
void set_email(string str) {
   log_file( "secure", "Email address changed from " + email + " to " + str + "\n" );
   email = str;
}
/// Set to 1 if the player wants their email address hidden.
void set_hide_email(int x) { hide_email = x; }
/** Sets the player's current directory. This will affect
 * wizards, mainly, since players do not use directory
 * navigation while they're playing.
 * \warning This function expects an absolute path. If you
 * wish to change to a relative path, such as ../obj, first
 * call make_path_absolute() and then pass the return
 * value to here. The cd command in cd.c uses
 * this function.
 */
void set_path(string str) { path = str; }

/** Sets the player's autohp preference. This is queried each time they
 * are dealt damage, and will force them to check their hp if this is
 * turned on.
 */
void set_autohp(int x) { this_object()->set_env_var("Tautohp", x); }

/** Sets the player's autoready preference. If turned on, they will see
 * their ready meter every heartbeat during combat.
 * There are 3 settings, 1-3 for low to high spam.
 */
void set_autoready(int x) { this_object()->set_env_var("Tautoready", x); }

/* Don't remove this set_name func or I will shoot you.  -= Venger =- */

/** Sets the name of this player. This is an overridden version of set_name,
 * modified so that only objects with root privileges may change the player's
 * name. This is important because the name is used in determining the file
 * name of the player's save file.
 */
void set_name(string str) {

   if( getuid(previous_object()) != MASTER_UID ) return;

   // Only objects with root privileges may set a player's name.
   name = str;
   if(level >= WIZLEVEL)
      msg_object(this_object(), "Your name has been changed to "+str+" by "+object_name(previous_object())+", this_player() is "+this_player()->query_cap_name());
}

/// Sets entire list of people to ignore.
void set_ignore( mapping x ) { ignore = x; }

/// Add a name to the ignore list.
void add_ignore( string name ) {
   if( !mappingp(ignore) ) ignore = ([ ]);
   ignore += ([ name ]);
}

/// Removes a name from the ignore list.
void remove_ignore( string name ) {
   if( mappingp(ignore) )
      ignore -= ([ name ]);
}

/// Change the contents of the tell history array
void set_tell_history(string *new) {
  tell_history = copy(new);
}

/// Add a new item to the tell history, and pop the oldest item if needed.
void add_tell(string who,string tell) {
  if ( !tell_history )
    tell_history = ({});
  else if ( sizeof(tell_history) == MAX_TELL_HISTORY )
    tell_history = tell_history[1..];
  tell_history += ({ ({who,ctime(time())[11..18],tell}) });
}

//set_say_history and add_say copied from set_tell_history and add_tell
// frobnitz 07 apr 2003
void set_say_history(string *new) {
   say_history = copy(new);
}

void add_say(string who,string say) {
  if ( !say_history )
    say_history = ({});
  else if ( sizeof(say_history) == MAX_SAY_HISTORY )
    say_history = say_history[1..];
  say_history += ({ ({who,ctime(time())[11..18],say}) });
}

/** Changes the contents of the memo mapping, I'm leaving the majority of
 * this code in the hands of the memo verb(s?) for the sake of sanity in
 * debugging and whatnot.
 */
void set_memo(mapping new) {
  memo = copy(new);
}

/** Change the number of warnings a player has. Do NOT call if you're
 * not a root UID object!
 */
void set_warnings( int x ) {
   if( getuid(previous_object()) != MASTER_UID )
      raise_error("Illegal set_warnings attempt from "+as_string(previous_object()));
   else
      warnings = x;
}

/// \return Sets the seconds since epoch time that the player first entered the game.
void set_creation_date( int x ) {
   if ( x < 0 ) x = 0;
   creation_date = x;
}

/// \return Set the seconds since epoch time that the player became a wizard. 0 if not a wizard.
void set_wiz_creation_date( int x ) {
   if ( x < 0 ) x = 0;
   wiz_creation_date = x;
}

/// \return sets the time the player has been idle, should only be called from /obj/living->heart_beat()
void set_idle_time( int t ) {
   idle_time = t;
}

//@}

/***************************************************************** PROMPTS */

/** @name Prompts
 * Functions for dealing with players' prompts.
 */

//@{
/** \return The player's current prompt. This is the string
 * printed to the terminal each time the player types a command
 * or pushes enter. If no prompt has been set, this function
 * returns a default one.
 *
 * See also query_default_prompt().
 */
string query_prompt() {
   if(!prompt) {
      prompt = "["+MUD_TITLE+"] ";
      default_prompt = prompt;
   }
   if(!format_prompt) format_prompt = process_string( prompt, 0 );
   return format_prompt;
}

/** Verifies that the prompt is set to something
 * valid and then calls the set_prompt efun to update
 * the prompt stored in the driver for this player.
 */
void update_prompt() {
   if(!prompt) prompt = query_prompt();
   format_prompt = process_string(prompt, 0);
   set_prompt(#'write_prompt, this_object());
}

/** Equivalent to write(this_player()->query_prompt()); */
void write_prompt() {
   write(query_prompt());
   command_mark = 0;
}

/** \return This player's "default" prompt. The default prompt
 * is the one that is restored when the interface stack exits
 * to the normal MUD interface.
 */
string query_default_prompt() {
   return default_prompt;
}

/** Sets the current prompt to \a str.
 *
 * \warning The value for str
 * may contain tilde codes, but they will be evaluated
 * immediately, not each time the prompt is displayed.
 * For example, if you place ~CDEF into the prompt,
 * and it is evaluated at nighttime and ~CDEF is a deep
 * blue, then when daytime comes the prompt will still
 * be blue. Closures are needed to fix this, but this
 * hasn't been done yet.
 */
void set_curr_prompt(string str ///< The string to be used as the new prompt. If blank or 0, the default prompt is used.
   ) {
   if(str)
   prompt = str;
   else prompt = default_prompt;

//   if(!prompt) prompt = query_prompt();
//   format_prompt = process_string(prompt, 0);
   //write("Trying to set prompt to: "+format_prompt+"\n");
//   set_prompt(format_prompt, this_object());
   update_prompt();
}

/** Sets the prompt to the default prompt, creating a default
 * prompt if necessary. This function actually updates the player's
 * prompt.
 */
void drop_prompt() { set_curr_prompt(0); }

/** Sets the default prompt to \a str, but doesn't actually
 * update the prompt. The default prompt is the one that
 * you return to when the player exits an interface.
 *
 * See reset_prompt().
 */
void set_default_prompt(string str) { default_prompt = str; }
//@}

/********************************************************* LOGON FUNCTIONS */


/** @name Player Creation
 */
//@{
/**
 * This function is called whenever the player object is created. It
 * behaves like a constructor. It fills in default values for the standard
 * aliases and the terminal size, then calls the create in its parent
 * object, /obj/living.c
 */
void create() {
   //string name;

   set_creation_date( time() );
   std_aliases = ALIAS_LIST;
   term_width = 79;
   term_height = 24;
   aliases = ([ ]);
   ::create();
}

/** Internal function, used for setting passwords. This function
 * returns 1, as it is also used as the new_password verb.
 * \todo This code uses input_to, one of the last pieces of
 * code on the MUD that does so unnecessarily. It could
 * be profitably rewritten to use an interface.
 */
int new_password(string f) {
   if(password) {
      write("Please enter your current password: ");
      input_to("new_password1", 1);
   } else new_password1("\n");
   return 1;
}

static varargs void new_password1(string str) {
   if(password && str != "\n")
   if(crypt(str, password) != password) {
      write("\nThat is not your current password.\n");
      return;
   }
   write("\nChoose a password ("+PASSWD_LEN+" or more letters, non-dictionary please!): ");
   input_to("new_password2", 1);
}

static void new_password2(string str) {
   if(!str || str=="")
   if(!password) {
      write("\nSorry, you can't log in without a password.  Good bye!\n");
      destruct(this_object());
      return;
   }
   else {
      write("\nPassword is unchanged.\n");
      return;
   }
   if(str == name || strlen(str) < PASSWD_LEN) {
      write("\nThat is an illegal password! Try again.\n");
      new_password1("\n");
      return;
   }

   newpass = crypt(str, 0);
   write("\nNow retype the password, to make sure they match: ");
   input_to("new_password3", 1);
}

static new_password3(string str) {
   if(crypt(str, newpass) != newpass) {
      write("\nYou typed something different the second time!  Try again.\n");
      new_password1("\n");
      return;
   }
   else {
      write("\nPassword set.  Don't forget it!\n\n");
      password = newpass;
   }

   save_self(PLAYER_DIR + name);
   if( creation_flag )
      this_object()->new_player_disclaimer();
}


/** An internal function, used during character creation. */
void new_player(string str) {
   creation_flag = 1;
   save_self();
   write("\nWelcome to " + MUD_TITLE + ", an LPMud running " + VERSION + ".\n\n");
   new_player_gender("");
}

static void new_player_gender(string str) {
   if (str == "m" || str == "M" || str == "male" || str == "Male") {
      set_gender(1);
      new_password1();
      return;
   } else if (str == "f" || str == "F" || str == "female" || str == "Female") {
      set_gender(2);
      new_password1();
      return;
   }
   write("Would you like to be Male or Female? ");
   input_to("new_player_gender");
}

static void new_player_disclaimer() {
   if (file_exists("/text/disclaimer")) {
      string file;
      file = read_file("/text/disclaimer");
      write(file+"\n");
      input_to("new_player_disclaimer_response");
   } else
      this_object()->new_player_color();
}

static void new_player_disclaimer_response(string str) {
   if (lower_case(str) != "i agree") {
      write("*** PLEASE READ DISCLAIMER AND FOLLOW INSTRUCTIONS ***\n");
      new_player_disclaimer();
   } else {
      msg("\nWalraven makes heavy use of color. Although the game will work "+
          "if you turn it off, it will be a lot more difficult to read the "+
          "maps and information screens. Most clients can handle color. If "+
          "you want to check that it will work, type 'test' below.\n\n");
      this_object()->new_player_color();
   }
}

void set_player_defaults() {
   add_env_var( "bin_paths", "/bin" );
   add_env_var( "bin_paths", "/bin/soul" );
   add_env_var( "help_paths", "/help" );
   this_player()->set_env_var( "Tmap", 1 );
   this_player()->set_readiness( 1000 );
   this_player()->set_food(500);
   this_player()->set_drink(500);
   this_player()->set_race("human");
   foreach ( string stat : ({ "str", "wil", "dex", "int", "con", "emp", "spd", "cha" }) )
      this_player()->set_stat(stat, 10);
   this_player()->setup_desc_var();
   command("skillme", this_player());
}

static void new_player_color( string s ) {
   if( !s ) {
      write("Do you want color [y/n/test]? ");
      input_to( "new_player_color" );
      return;
   } else {
      s = lower_case(s);
   }

   if( s == "yes" || s == "" || s == "y" ) {
      set_ansi(1);
      msg( "Color turned ~CLABON~CDEF. Use the ~CCOMansi~CDEF command to change it.");
   } else if( s == "no" || s == "n" ) {
      set_ansi(0);
      msg( "Color turned OFF. Typing 'ansi on' later will turn it on.");
   } else if( s == "test" ) {
      set_ansi(1); msg("~[090TEST~CDEF"); set_ansi(0);
      msg("If you see the word 'TEST' on a line by itself, answer yes. "+
          "It should be red, but it might not be on some clients. If "+
          "there is other 'junk' next to the word, your client is getting "+
          "confused by the color codes. You should then answer no.");
      write("Do you want color [Y/n/test]? ");
      input_to("new_player_color");
      return;
   } else {
      msg("Please answer yes, no, or test.");
      input_to("new_player_color");
      return;
   }

   set_player_defaults();
   enter_world();
}

/** Adds some standard verbs to the player object. Verbs inherent
 * in the player object are difficult to edit, but have the advantage
 * that even if the bin verbs are broken, these will still work.
 * It is also organizationally simpler to put some verbs in the
 * player, and it is ideal for verb abbreviations. You should
 * look at the source code to see the list of verbs added.
 */
void add_player_commands() {
   add_action("search_bin", "", 1);
   add_action("short_say", "'", 1);
   add_action("short_emote", ":", 1);
   add_action("short_call", "@", 1);
   add_action("alias", "alias");
   add_action("alias", "unalias");
   add_action("quit", "quit");
   add_action("new_password", "password");
   add_action("emergency_shutdown", "emergency_shutdown");
   add_action("null_action", "null_action");
}

#ifdef __MCCP__
/**
 * Start the player in MCCP mode
 */
void mccp_message();

void disable_mccp() {
   end_mccp_compress();
}

void do_mccp() {
   if( !query_mccp() ) {
      binary_message( ({ IAC, WILL, TELOPT_COMPRESS2 }), 1 );
      binary_message( ({ IAC, WILL, TELOPT_COMPRESS }), 1 );
   }
   call_out("mccp_message",2);
}

/**
 * Let the player know whether they wound up compressed or not
 */
void mccp_message() {
   switch( query_mccp() ) {
      case TELOPT_COMPRESS:
         msg("Your client supports old style MCCP 1 compression; using it.");
         break;
      case TELOPT_COMPRESS2:
         msg("Your client supports MCCP 2 compression; using it.");
         break;
      default:
         //msg("Your client does not support any compression; automatically disabling.");
   }
}
#endif

#ifdef __TLS__
/*
void do_tls() {
   tls_init_connection(this_object());
}
*/
#endif

/** A function that is called every time a player logs in. It handles
 * initialization of the player and the standard objects in his
 * inventory (currently the chat and security objects). It also
 * performs simple functions like printing the news and checking
 * for new mail.
 */
varargs void enter_world(int reentry
/**< The reentry flag determines what type of a login
 * this is. Valid values are
 *  - 0 for a standard log in.
 *  - 1 for a player returning after they have been linkdead
 *  - 2 for a "silent" log in, used by the flash command to
 *    recreate the player object.
 */
   ) {
   object my_secure, my_chat;
   string news;
   string curr_ip = query_ip_name() + " ("+query_ip_number()+")";

   seteuid( getuid(this_object()) );
   add_player_commands();
   set_living_name(name);
   set_curr_prompt(default_prompt);

   remove_call_out("time_out");


#ifdef __MCCP__
   do_mccp();
#endif

   if( query_env_var("skill_patchlevel") != "/bin/skillme"->query_patchlevel() ) {
      msg("~CWRNYour skills are out of date. Running ~CBRTskillme~CWRN automatically.~CDEF");
     "/bin/skillme"->main();
   }

   if( reentry != 2 && !creation_flag ) {
      // check mail
     int unread;
     if ( catch( unread = "/secure/mail_daemon"->query_newmsgs( name ) ) ) {
         msg("~CWRNSomething went wrong trying to check your mail.~CDEF");
     }
   // old way:
   //   display_file(TEXT_DIR + "news.cod");
   // new way
      catch( "/bin/news"->main() );
      if( unread )
         msg("~CTITYou have mail.~CDEF");
      // "WCLIENT" in morse code. ;-) If the client receives this,
      // it'll try turn on client codes. \t is dash, space is dot,
      // \b is pause.
      msg( "\t  \b \t \t\b\t \t\t\b\t\t\b\t\b \t\b " );
   }

   if(reentry != 1) {
      if(catch(my_secure = clone_object("/secure/sub_sec_daemon"))) {
        write("secure/*** A system file failed to load.\n");
        destruct(this_object());
        return;
      }
      if(catch(my_chat = clone_object("/obj/chat")))
         write("obj/chat: Your chat object failed to load, so you will not be\n"+
               "able to chat. Use shout instead.\n");
      if(my_chat) {
      my_chat->move(this_object());
        my_chat->init();
      }
      if( my_secure ) {
         my_secure->move(this_object());
      }

//   msg("SET last_login TO "+(string)time()+" ("+ctime(time())+")");
      if( reentry != 2 ) {
         string dupes = "";
         object who;
         if (this_object()->query_level() < WIZLEVEL && !this_object()->query_testchar()) {
            foreach ( who : users() ) {
               if (who->query_level() >= WIZLEVEL ||
                   who->query_testchar() ||
                   who == this_object())
                  continue;
               if (query_ip_number(who) == query_ip_number(this_object()))
                  dupes += who->query_cap_name()+", ";
            }
            if (dupes != "")
               dupes = dupes[..<3];
         }

         foreach( who: users() ) {
            if (who->query_level() >= WIZLEVEL) {
               if (query_invis())
                  msg_object(who, "~CLAB"+capitalize(name)+" enters the game from "+query_ip_name(this_object())+" [invis].~CDEF");
               else {
                  if (last_login)
                     msg_object(who, "~CLAB"+capitalize(name)+" enters the game from "+query_ip_name(this_object())+".~CDEF");
                  else
                     msg_object(who, "~CLAB"+capitalize(name)+" enters the game for the first time, from "+query_ip_name(this_object())+".~CDEF");
               }
               if (dupes != "")
                  msg_object(who, "~CWRNOther players connected from that same ip: "+dupes+"~CDEF");
          } else if (query_invis() != 1) {
               if (last_login)
                  msg_object(who, "~CLAB"+capitalize(name)+" enters the game.~CDEF");
               else
                  msg_object(who, "~CLAB"+capitalize(name)+" enters the game for the first time.~CDEF");
            }
         }
         if( query_invis() != 1 )
           catch( IRCD->emote( name, "enters the game"+(!last_login?" for the first time":"") ) );
       // tell the player when they last logged in.
         if (last_logout)
          msg("It has been "+int_to_time(time()-last_logout)+" since your last visit.");
         last_login = time();
      }
      if( (!location || !find_objectref(location) || !move_player(find_objectref(location), 0)) && (!current_location || !move_player(current_location) ) && !creation_flag )
         move_player(";wizarea;reception");
      else if (creation_flag)
         move_player(";witac;peak");
   }
   else {
      last_login = time();
      say_all("~CLAB~Name returns from the land of the linkdead.~CDEF");
   }

   if( reentry && last_ip != curr_ip )
     log_file("entrylog", "Relog from "+curr_ip+"\n");
   if( reentry == 0 )
     log_file("entrylog", "Login from "+curr_ip+"\n");

   last_ip = curr_ip;

   // force a food tick
   call_out( "decay_food_and_drink", 2 );

   recalc();
   catch("/daemon/nolife"->login(this_object()->query_name()));
   catch("/daemon/chat"->login(this_object()));
   catch("/daemon/party"->login(this_object()->query_name()));
   set_modify_command(this_object());
   catch("/bin/who"->wholist_dump());

   // start daemons if they'r ethe first player to log in
   if( sizeof(users()) == 1 ) {
      debug("only player online, firing up daemons");
      catch("/daemon/online"->create());
   }

   // initialize them for sound
   AUDIO_DAEMON->start_the_music( this_object() );
}

void time_out() {
   write("\nTime Out.\n");
   destruct(this_object());
}
//@}

/**************************************************** PLAYER-ONLY COMMANDS */

/** @name Verbs
 */
//@{
void alias_line(string key) {
   write(process_string("|~CLAB"+pad(key,11,1)+"~CDEF | ", 0)+pad(aliases[key],term_width-18,0)+" |\n");
}

mapping query_alias() { return aliases; }

/** This function handles the alias command. It can print a list
 * of current aliases, or set up a new alias. This could be made
 * a /bin verb.
 */
int alias(string str) {
   string akey, aval;

   if(!aliases) aliases = ([]);

   string *akeys = sort_array(m_indices(aliases), #'>);

   if(!str) {
      msg_bare(" ____________\n");
      msg_bare("| ~CTITAlias list~CDEF |" + tab("_", term_width-16) + "\n");

      map(akeys, "alias_line", this_object());

      msg_bare("|____________|" + tab("_", term_width-16) + "|\n");
      return 1;
   }

   sscanf(str, "%s %s", akey, aval);

   if (query_verb() == "alias" && akey && aval &&
       ( akey[0..0] == "!" || aval[0..0] == "!" ) ) {
      msg("==> [Alias] Aliases cannot begin with a !, see 'help alias'.");
      return 1;
   }

   if ( query_verb() == "unalias") {
      if (member(aliases, str))
         msg("==> [Alias] Entry for " + str + " was deleted.");
      else
         msg("==> [Alias] Entry for " + str + " does not exist.");
      efun::m_delete( aliases, str );
      return 1;
   }

   if (akey == "alias" || akey == "unalias") {
      write("==> [Alias] For sanity's sake, aliasing 'alias' and 'unalias' is not allowed.\n");
      return 1;
   }

   if (!aval) {
      if (member(aliases, str) == 0)
         msg("==> [Alias] Entry for " + str + " does not exist.");
      else {
         write("==> [Alias] "+str+" = "+aliases[str]+"\n");
         msg("To remove an alias, use the ~CCOMunalias~CDEF command.");
      }
      return 1;
   }

   aliases[akey] = aval;
   msg("==> [Alias] Entry added for "+akey+".");

   return 1;
}

static emergency_shutdown() {
   if(query_level() < ADMLEVEL) { return 0; }

//   shout("(obj/player) shouts: Emergency shutdown called!  Good bye!\n");
   shutdown();
   return 1;
}

/** This is the inventory bubble code. It moves your inventory to a bubble
 * where it is kept until you log in again.
 */
void make_bubble() {
   object ob, bubble, inv, bank;

   if( !(bank = find_room("bank")) )
      debug("The area 'world;bank' must exist for players to save inventory! It should be a clone of /obj/bank.");
   // Make sure a bubble exists
   if( !(bubble = present(name, bank)) ) {
      bubble = clone_object("/obj/invbubble");
      bubble->set_name(name);
      bubble->move(bank);
   }
   // Move all the objects
   foreach( ob : all_inventory(this_object()) )
      ob->move( bubble );
   bubble->save_self();
}

/** Restore a player's inventory to him from the inventory bubble where
 * it has been sitting.
 */
void restore_bubble() {
   object ob, bubble;

   if( bubble = find_room(";bank;"+name) ) {
      foreach( ob : all_inventory(bubble) )
         ob->move( this_object(), 1 ); // No events!
      destruct( bubble );
   }
}

/** This player quits the game. The quiet variable
 * may be set to several different values to reflect
 * the reason for quitting.
 *
 * \param quiet This variable may be set to:
 * - 0 to quit normally
 * - 1 to quit normally, but with absolutely no messages.
 * - 2 to quit without updating the log files. This is used
 *   by the flash command.
 * - 3 to quit because the player has become linkdead. This
 *   is not actually quiet, but displays a different message
 *   indicating that the player is linkdead.
 */
void quit( int quiet ) {
   if( !intp(quiet) ) {
      notify_fail("If you're trying to quit, just type the command by itself.\n");
      return 0;
   }
   catch("/daemon/nolife"->logout(this_object()->query_name()));
   catch("/daemon/chat"->logout(this_object()));
   catch("/daemon/party"->logout(this_object()->query_name()));
   // stop daemons if they'r ethe last player to log out
   if( sizeof(users()) <= 1 ) {
      debug("last player logging out, killing daemons");
     catch(destruct(find_object("/daemon/online")));
   }
   // update who list for web page
   catch("/bin/who"->wholist_dump());
   if (query_invis() != 1) {
      if(!quiet) say_all("~CLAB" + query_cap_name()+ " leaves the game!~CDEF");
      if( quiet == 3 ) say_all("~CLAB" + query_cap_name() + " wanders off to the land of the linkdead.~CDEF");
      if( quiet != 2 )
         catch( IRCD->emote( name, "leaves the game" ) );
   }
   //msg("Saving player data...");
   msg_object(this_object(), "Saving player data...", 0);

   if( quiet != 2 ) {
      last_logout = time();
      if( last_login <= 0 ) last_login = time();
      age += last_logout - last_login;
   }

   // Warn objects that I'm quitting.
   object ob;
   foreach( ob : deep_inventory() )
      ob->on_suspend();
   make_bubble();
   save_self();

   msg_object(this_object(), "Saving complete.", 0);
   if( quiet < 2 || quiet == 3 ) {
      string *qmsg_dir = get_dir(TEXT_DIR+"quitmsg.dir/quitmsg.*");
      int qmsg_index = random( sizeof(qmsg_dir)), x;
      string qmsg_file = read_file(TEXT_DIR+"quitmsg.dir/"+qmsg_dir[qmsg_index]);
      string bar = "";
      for (x = 0; x < query_term_width() - 2; x++)
        bar += (x%2?"=":"-");
      qmsg_file = "~CLAB"+bar+"~CDEF\n"+qmsg_file+"~CLAB"+bar+"~CDEF\n";
      msg_object(this_object(), process_string(qmsg_file, 0), 0);
   }
   log_file("exitlog", "Logout stamped.\n");
   destruct(this_object());
}

mixed destructor( object ob ) {
   if( object_name(this_object()) != "/obj/player" && ob != this_object() )
      return "Illegal destruct attempt. Use the 'quit' function to destruct player objects.";
   ::destructor( ob );
   return 0;
}

/** Searches the player's list of aliases and if it finds a match, it executes it.
 * Not intended for outside calls.
 */
int search_alias(string params) {
   if(!aliases) aliases = ([]);
   if(aliases[query_verb()]) {
      if(params)
      command(aliases[query_verb()] + " " + params);
      else
      command(aliases[query_verb()]);
      return 1;
   }
   return 0;
}

/** Searches through the bin verbs given in bin_paths and returns an array
 * of all matching object files. If no matches are found, an empty array
 * is returned. The .c's are omitted from the object file names.
 */
string *locate_command(string command) {
   int pathloop;
   string * bin_paths, a, b;
   string * file_matches;
   object fobj, ob;
   string str;
   if(!command)
      return 0;

   bin_paths = query_env_var("bin_paths");
   if(!bin_paths) {
      add_env_var("bin_paths", "/bin");
      bin_paths = query_env_var("bin_paths");
   }
   if(level >= WIZLEVEL) {
      if(member(bin_paths, "/bin/wiz") < 0) {
         msg("==> [Notice] Added /bin/wiz to your executable path.");
         add_env_var("bin_paths", "/bin/wiz");
         bin_paths = query_env_var("bin_paths");
      }
   }

   file_matches = ({ });
   //Check for an exit named the same as the command, gets that
   //shortcut bug gone.
   if(environment(this_object())){
      foreach( ob : filter(all_inventory(environment(this_player())),
                          (: $1->query_is_exit() :) ) ){
         if( stringp(str = ob->query_name()) && (str[5..] == command) ) {
            file_matches += ({ "/bin/go" });
         }
      }
   }
   // NEW FEATURE:
   // Check their combat form actions first in order to override any other
   // commands and finally allow people to specify battle actions in stead of
   // just hoping the RNG favors them - Al [Aug 3, 05]
   fobj = this_object()->query_form_object();
   if( member(fobj->query_all_actions() - fobj->query_reactions(), command) != -1 )
      file_matches += ({ "/battle/actions/"+command });

   // actually search their bin paths now
   for(pathloop = 0; pathloop < sizeof(bin_paths); pathloop++) {
//      write("Checking path: "+bin_paths[pathloop]+" with verb "+command+"\n");
      if(file_exists(bin_paths[pathloop]+"/"+command+".c") > 0) file_matches += ({ bin_paths[pathloop]+"/"+command });
   }

   if( sizeof(file_matches) == 0 ) return 0;
   return file_matches;
}

int search_bin(string params) {
   string command, ibin;
   string *bin_list;
   int ret;

   command = query_verb();
   if( sscanf(command, "%~s.%~s") == 2 || sscanf(command, "%~s/%~s") == 2 ) return 0;

   bin_list = locate_command( command );
   if( !pointerp(bin_list) ) return 0;
   foreach( ibin : bin_list )
      if( ret = call_other(ibin, "main", params) )
         return ret;
   return 0;
}


/** Used for the short form of the say command ('). Not intended for outside calls. */
int short_say( string param ) {
   if( query_verb() == "'" && !param )
      return "/bin/say"->main( 0 );

   if( param ) param = query_verb() + " " + param;
   else param = query_verb();
   param = param[1..strlen(param) - 1];
   return "/bin/say"->main( param );
}

/** Used for the short form of the emote command (:). Not intended for outside calls. */
int short_emote( string param ) {
   if( query_verb() == ":" && !param )
      return "/bin/emote"->main( 0 );

   if( param ) param = query_verb() + " " + param;
   else param = query_verb();
   param = param[1..strlen(param) - 1];
   return "/bin/emote"->main( param );
}

/** Used for the short form of the call command (@). Not intended for outside calls. */
int short_call( string param ) {
   if( this_player()->query_level() < WIZLEVEL ) return 0;

   if( query_verb() == "@" && !param )
      return "/bin/wiz/call"->main( 0 );

   if( param ) param = query_verb() + " " + param;
   else param = query_verb();
   param = param[1..strlen(param) - 1];
   return "/bin/wiz/call"->main( param );
}

/** Does nothing. \return 1 */
int null_action() {
   return 1;
}

/** An internal MUDlib function used to parse aliases,
 * keep a command history, and so on. This function
 * is called by the driver each time a command is typed
 * by this player. It is nonsensical to call it otherwise.
 */
nomask string modify_command(string comm) {
   string verb, param;
   int i;

   command_mark = 1;
   // ... This is a good place to check if interfaces have broken,
   // and if so to fix them. However, I can't remember the last
   // time that happened... :-b. Seems I got the code right this time.

   if( file_size("/log/comm") == -2 )
      log_file( "comm/"+this_object()->query_name(), comm + "\n" );

   // silly hack to prevent this message from showing up when the
   // player is actually still idle but has a command forced on them
   if( idle_time > 1800 && query_idle(this_player()) < 10 )
      msg_object(this_player(), "You were idle for " + int_to_time(idle_time) + ".");
   idle_time = 0;

   if( comm ) {
      comm = trim( comm );
      if( !command_history || sizeof(command_history) != COMMAND_HISTORY_SIZE )
         command_history = allocate( COMMAND_HISTORY_SIZE );
      if( icommand == COMMAND_HISTORY_SIZE ) icommand = 0;

      // Decide whether to activate the 'quicktyper'
      if( comm[0..1] == "%%" ) {
         // Quicktype...
         param = comm[2..];
         if( param == "" ) comm = command_history[icommand];
         else if( param == "list" ) {
            msg_bare("~CTITCommand history\n"
                   + "~CDEF---------------\n");
            i = icommand;
            if( command_history[i] )
            do {
               if( command_history[i] )
                  msg_bare((string)((COMMAND_HISTORY_SIZE + icommand - i) % COMMAND_HISTORY_SIZE + 1)+". "
                           + command_history[i]+"\n");
               i--;
               if( i < 0 ) i = COMMAND_HISTORY_SIZE - 1;
            } while (i != icommand);
            else msg_bare("No commands in history.\n");
            notify_fail("");
            return "null_action";
         }
         else if( i = to_int(param) ) {
            if( i <= COMMAND_HISTORY_SIZE ) {
               comm = command_history[(COMMAND_HISTORY_SIZE + 1 + icommand - i) % COMMAND_HISTORY_SIZE];
            }
         }
      }
      else
         command_history[(++icommand)%COMMAND_HISTORY_SIZE] = comm;
   }
   if( stringp(comm) && sscanf( comm, "%s %s", verb, param ) != 2 ) {
      verb = comm;
      param = 0;
   }
   if( aliases[verb] ) {
      comm = "/daemon/parse_alias"->parse_alias(aliases[verb], param);
      if( stringp(comm) && sscanf( comm, "%s %s", verb, param ) != 2 ) {
         verb = comm;
         param = 0;
      }
   }
   if( std_aliases[verb] ) comm = std_aliases[verb] + (param?" " + param:"");

   if( comm )
      return comm;
   else return "null_action";
}

/** Returns an array of recently typed commands. The most
 * recent is not necessarily the first in the array, as it
 * is cyclical.
 */
string *query_command_history() {
   if( query_level() < WIZLEVEL || getuid(previous_object()) == getuid(this_object()) || getuid(previous_object()) == "root" )
      return command_history;
   else
      return ({ "Sorry, this person's command history is private." });
}

/// Returns the last command typed by the player
string query_last_command() {
   if( query_level() < WIZLEVEL || getuid(previous_object()) == getuid(this_object()) || getuid(previous_object()) == "root" ) {
      if (sizeof(command_history) > icommand)
         return command_history[icommand];
      else
         return 0;
   } else
      return ({ "Sorry, this person's command history is private." });
}
//@}

/******************************************************** SYSTEM FUNCTIONS */

/** @name File access */

//@{
/** Dumps the contents of a file to this_player(). This function is not
 * designed to work with very long files. It doesn't dump the
 * file contents to this_object().
 */
varargs void display_file(string name, ///< Full path of the file to dump to the terminal.
   int line ///< Zero-indexed line to start dumping from. Defaults to 0.
   ) {
   string buf;

   if( !stringp(name) ) return 0;
   buf = read_file(name, line+1, 10);
   if(!buf) return;
   buf = process_string(buf, 0);
   write(buf);
   display_file(name, line+10);
}

/** (Obsolete) Checks read access and resolves a file's full path.
 * \warning This function is obsolete. You should not be using it in
 * new code. It issues a warning on the debug() channel whenever it is
 * called (see chat.c). Its functionality has been completely replaced
 * by make_path_absolute() and query_read_access().
 */
mixed valid_read(string str, mixed path) {
   string d1, d2, temp, temp2, temp3;
   int i,check;

   debug("valid_read called in player "+name+" by object "+object_name(previous_object()));

   temp3 = "";
   check = 0;
//   debug(this_player()->query_name()+"->valid_read("+str+","+path+") from "+object_name(previous_object()));
   if(path == 1) path = query_path();
   if(!path) path = "/";
   if(path[strlen(path)-1]=='/') path = path[0..strlen(path)-2];
   if(!str) {
      str = path;
      path = "/";
   }

   /* Make sure path is based from root */
   if(path[0] != '/') path = "/" + path;

   /* Remove trailing /'s from path */
   while(path[strlen(path) - 1] == '/' && strlen(path) > 1) path =
         path[0..strlen(path) - 2];
   temp = path;
   /* If root not specified, use path */
   if(str[0] != '/') str = path + "/" + str;

   /* Simplify ".." scanning, i.e. /y/../z -> /z, etc. */
   while(sscanf(str, "%s/..%s", d1, d2) == 2)
   if(d1 == "") str = d2;
   else {
      //i = strlen(d1) - 1;
      i = strlen(d1);
      while(i > 0 && d1[i] != '/') i--;
      if(i == 0) d1 = ""; else d1 = d1[0..i-1];
      str = d1+d2;
      if(str=="" || !str) str = "/";
   }

   /* Change // to / ... necessary esp. if path was root (/+/+str) */
   while(sscanf(str, "%s//%s", d1, d2) == 2) str = d1 + "/" + d2;

   if(level >= ADMLEVEL) return str;

   check = 1;
   if(!check) return 0;
   return str;
}

/** (Obsolete) Checks write access and resolves a file's full path.
 * \warning This function is obsolete. You should not be using it in
 * new code. It issues a warning on the debug() channel whenever it is
 * called (see chat.c). Its functionality has been completely replaced
 * by make_path_absolute() and query_write_access().
 */
mixed valid_write(string str, mixed path) {
   string d2, temp, temp2, temp3;
   string file, d1;
   int i;

   debug("valid_write called in player "+name+", why!?");

//   if(!allow_write) allow_write = ({});
   if(!path) path = query_path();
   if(!path) path = "/";
   if(path[strlen(path)-1]=='/' && strlen(path)>1) path = path[0..strlen(path)-2];

//   debug(this_player()->query_name()+"->valid_write("+str+","+path+")");
   // Note: Auto-denies files you can't read...
   file = valid_read(str, path);
//   debug("valid_read returns: "+file);

   // Admins get a freeby here...
   if(level >= ADMLEVEL) return file;

   if(!file) return 0;

   // Start of DON'T KILL THIS OR Players won't be able to SAVE!!!
   // ';/
//   write("File is "+file+"\n");
//   write("Dir is "+PLAYER_DIR+name+"\n");
   if(file==PLAYER_DIR+name || file=="/"+PLAYER_DIR+name) return file;

   // End of don't kill section

   return file;
}

/** Converts a relative path reference to a global one. This function
 * will recognize such things as ~'s for home directories and .. or . for
 * previous and current directory. The file need not exist for make_path_absolute
 * to work; its rules are independent of the contents of the file system.
 *
 * This function is intended to replace valid_read() and valid_write() in player.c
 * Currently the only thing real purpose of this valid_read/write pair is
 * to make paths absolute. When they are deleted, this function should be
 * used instead.
 */
string make_path_absolute( string str ///< The path to be made absolute
   ) {
   string start_path, a, b, c;
   int i;

   if( !stringp(str) ) return 0;

   if( str == "." ) return query_path();
   start_path = query_path() + "/";
   if( start_path == "//" ) start_path = "/";

   if( str == "~" ) return "/home/"+name;
   if( str[0] == '/' ) start_path = "";
   if( sscanf(str, "~/%s", str) == 1 )
      start_path = "/home/" + name + "/";
   else
      if( sscanf(str, "~%s/%s", a, str) == 2 ) start_path = "/home/" + a + "/";
   else
      if( sscanf(str, "~%s", a) == 1 ) str = "/home/" + a + (start_path="");

   str = start_path + str;

   // Remove 'up one directory' markers from mid-string.
   while( sscanf( str, "%s/..%s", a, b ) == 2 ) {
      i = strlen(a);
      while( i > 0 && a[i] != '/' ) i--;
      if( i == 0 ) str = b;
      else
         str = a[..i-1] + b;
   }

   // Remove 'current directory' markers
   while( sscanf(str, "%s/./%s", a, b) == 2 )
      str = a + "/" + b;

   return str;
}

protected void load_single_item( string base, int id ) {
   object ob;
   ob = clone_object( base );
   if( ob ) {
      ob->load_self( id );
      ob->move( this_object() );
   }
}

/** Loads a .o file's values into this player. This function must
 * be called from an object which has root privileges, or it will
 * not work.
 * \return True on success (the values loaded), false on failure.
 */
int load_self(string str) {
   string err;

   if( getuid(previous_object()) != MASTER_UID ) return 0;
   seteuid(getuid(this_object()));
   if(!str || str == "") str = name;
   if(!restore_object(PLAYER_DIR + str)) return 0;

   restore_bubble();
   object ob;
   foreach( ob : all_inventory() )
      ob->on_restore();

   /** This new bit of code calculates the player's 'level' from
    * their total skill ranks every time they log in. Since level
    * really doesn't serve much of a purpose... it's more of a
    * nicety than anything else, but it will allow for some vague
    * form of comparrison between players and will allow for some
    * custom level strings on the who listing. - Al [3.6.02]
    */

   if (this_object()->query_level() < WIZLEVEL) {
     int skill_total = this_object()->calculate_skill_total();
     int new_level = this_object()->calculate_level(skill_total);
     if (new_level != level && new_level < WIZLEVEL) {
       // write("Level set to "+new_level+".\n");
       this_object()->set_level(new_level);
     }
   }

   return 1;
}

/** Saves this player's settings to the player's save file.
 * The name of the save file is automagically determined,
 * and is based on the player's name. Anyone may call this
 * function, it is quite safe. This also calls save_self
 * in all the objects in the player's inventory and saves
 * them instantly.
 * \todo When inventory saving is no longer implemented
 * by a call_out (in saveload.c), this function no longer
 * needs to use deep_inventory to save the player's inventory.
 */
void save_self() {
   object ob, bubble;
   int i;
   string str;

   current_location = get_location(environment(this_object()));
   if( environment() )
      location = to_objectref( environment() );
   str = PLAYER_DIR + name;

   // Forces 'simple' file format, since these files are read
   // by external programs.
   save_object(str);
}

symbol query_location() {
   return location;
}
//@}

/** @name Messaging */
//@{

/// Used to initialize portal-style mud clients
varargs void client_init( int code, string version ) {
   client_code = as_string(code);
   while( strlen(client_code) != 5 ) client_code = "0" + client_code;
   client_ver = version;
}

/** An internal function used to figure out whether received text
 * is being handled by an interface.
 */
static int find_catcher() {
   int i;

   for( i = 0; i < sizeof(imode); i++ )
      if( imode[i] & 0x02 ) return i;

   return -1;
}

/// Handles messages passed to the player.
void catch_tell( string str, object src, int line_clean ) {
   int catcher;

   if( !interactive(this_object()) ) return;

   if( !mappingp(env_vars) ) env_vars = ([ ]);
   // src is a hook to allow simul_efun to fake the previous_object().
   if( !src )
      src = previous_object();
   if( line_clean ) {
      if( !command_mark && env_vars["Tautobreak"] )
         str = "\n" + str;
      command_mark = 1;
   }
   if(!_in_receive && src != query_interface() )
   if( pointerp(imode) )
   if( (catcher = find_catcher()) != -1 ) {
      _in_receive = 1;
      _interface_catch( str, catcher );
      _in_receive = 0;
      return;
   }
   efun::tell_object( this_object(), str );
}

////////// Intermud-3 Kludge ///////////
// Note: This method is going to go bbye whenever work on I3 is complete
void receive_message(string class, string msg)
{
   string channels = this_object()->query_env_var("i3_channels");
   debug(this_object()+" >> receive_message("+class+","+msg+")","i3");
   if (channels && member(channels, class) != -1) {
     int length = member(msg,']') - 8;
     msg_object(this_object(),msg+"~CDEF", length);
   }
}

//@}
