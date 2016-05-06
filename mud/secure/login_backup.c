#include <const.h>
#include <file_security.h>

/// An object that handles player's logins.

object newplayer;
string name, password; /**< The player's password. */
int last_login, level;

int tries;
int client_code;
int want_invis;
int want_vis;
string client_ver;

// This file is not included in the autodocs, so the
// password is relatively safe ;-).
#include "/secure/shutdown_password"

int get_info( string name );
void enter_world();
static void brief_prompt(string arg);

void create() {
   tries = 1;
   seteuid("root");
}

static int logon( int quiet ) {
   string intro_file;

   call_out("time_out", 120);

   if ( !quiet ) {
      intro_file = read_file(TEXT_DIR + "title" + (random(NUM_TITLES) +
                             1) + ".txt");
      write(intro_file);
   }

   client_code = -1;
   client_ver = 0;

   write("\nYour name (or 'who'): ");
   input_to("get_name");
   return 1;
}

static void get_name(string arg) {
   if( arg[..5] == "#$#mcp" ) {
      "/secure/mcp"->on_mcp( arg[7..] );
      input_to("get_name");
      return;
   }

   if( sscanf(arg, "3klient %d~%s", client_code, client_ver) == 2 ) {
      write("PORTAL version "+client_ver+" detected, code was "+client_code+"\n");
      input_to("get_name");
      return;
   }

   arg  = lower_case(arg);
   name = lower_case(arg);

   if(!arg || arg == "") {
      write("Another time, then!\n");
      destruct(this_object());
      return;
   }

   want_invis = 0;
   want_vis = 0;

   if( arg[<2..] == "+i" ) {
      want_invis = 1;
      name = arg = arg[..<3];
   } else if( arg[<2..] == "-i" ) {
      want_vis = 1;
      name = arg = arg[..<3];
   }

   if(arg == "shutdown*") {
      write("WARNING: You have requested a login mode shutdown.\n");
      write("This should only be used if a bug in the player object is\n");
      write("preventing you from logging in! Enter the password:");
      input_to("external_shutdown", 1);
      return;
   }

   if(arg == "who") {
      "/bin/who"->plain_wholist();
      input_to("logon");
      return;
   }
   if(arg == "quit") {
      msg("Bye now!\n");
      destruct(this_object());
      return;
   }
   if(arg == "web connector*") {
      object web_connector;

      catch(web_connector = clone_object("/secure/web_connector"));
      if( !web_connector) {
         write("Error cloning web connector\n");
         destruct(this_object());
         return;
      }
      exec(web_connector, this_object());
      web_connector->notify_connect();
      destruct(this_object());
      return;
   }

   if( "/secure/banned_names"->query_banned(arg) || !legal_filename(arg)
       && arg != "shutdown*" ) {
      write( "Sorry, that is an illegal name and you may not use it. Choose a different one.\nYour name: " );
      input_to( "get_name" );
      return;
   }

   if(find_player(name)) {
      write("Someone by that name is already in the game! If you are a new\n");
      write("player, you will need to select a new name (just push Enter).\n");
      write("If you went link-dead, enter your password to get back in: ");
      msg_object( find_player(name), "~CBRTSomeone is attempting to login as you from "+query_ip_name()+" ("+query_ip_number()+").~CDEF" );
      log_file( "linkdead", "Linkdead login for "+name+" from "+query_ip_number()+" ("+query_ip_name()+")\n");
      get_info(name);
      input_to("get_password", 1);
      return;
   }

   if(get_info(name)) {
      if( level < WIZLEVEL ) {
         want_invis = 0;
         want_vis = 0;
      } else if ( want_invis )
         write("You will log in invisibly.\n");
      else if ( want_vis )
         write("You will log in visibly.\n");

      if(password) {
         write("Last login was "+ctime(last_login)+"\n");
         write("Welcome back, " + capitalize(name) +
               ", what is your password? ");
         input_to("get_password", 1);
      }
      else {
         write("This character has no password!\n");
         enter_world();
      }
   } else {
      write("I don't recognize you. Are you a new player (Y/n)? ");
      input_to("ask_is_new");
   }
}

static void external_shutdown(string pass) {
   if(pass != SHUTDOWN_PASSWORD) {
      write("You may not use external shutdown without the password.\n");
      destruct(this_object());
      return;
   }
   remove_call_out("time_out");
   brief_prompt(0);
}

static void kill_it() {
   shutdown();
}

static void brief_prompt(string arg) {
   if(!arg || arg == "") {
      write("Shutdown menu:\nshutdown -- Shutdown with save\n"+
            "shutdown! -- Fast shutdown\n"+
            "shout -- EchoAll a message to warn players.\n"+
            "quit -- Never mind, leave this mode and exit.\n");
      write("[Shutdown] ");
      input_to("brief_prompt");
      return;
   }
   if(arg == "shutdown") {
      call_out("kill_it", 60);
      write("MUD is being told to shut down. Timeout is in one minute.\n");
      call_other("obj/shut", "pull_the_plug");
   }
   if(arg == "shutdown!") kill_it();
   if(sscanf(arg, "shout %s", arg) == 1) msg_all("The MUD driver shouts: "+arg);
   if(arg == "quit") {
      write("So much fuss, so little result.\n");
      destruct(this_object());
   }
   input_to("brief_prompt");
   write("[Shutdown] ");
}

static void ask_is_new(string arg) {
   if(arg == "" || arg == 0 || arg[0] == 'n' || arg[0] == 'N') {
      write("Ok, then what is your name? ");
      input_to("get_name");
      return;
   }

   level = 0;
   newplayer = clone_object("obj/player");
   newplayer->set_name(name);
   seteuid( name );
   export_uid( newplayer );
   exec(newplayer, this_object());
   newplayer->new_player();
   destruct(this_object());
}

void get_password(string arg) {
   if(!arg || arg == "") {
      input_to("logon");
      return;
   }

   if(crypt(arg, password) != password) {
      write("\nWrong password!\n");
      log_file("badpassword", "Wrong password for "+name+" from "+query_ip_number()+" ("+query_ip_name()+")\n");
      if (tries < 3) {
         tries++;
         write("Password? ");
         input_to("get_password", 1);
         return;
      } else {
         destruct(this_object());
         return;
      }
   }

   remove_call_out("time_out");
   printf("\n");
   enter_world();
}

int get_info(string name) {
   if( !legal_filename(name) )
      return 0;
   return restore_object(PLAYER_DIR + name);
}

void time_out() {
   write("\nTime Out.\n");
   destruct(this_object());
}


void enter_world() {
   int isnew;

   isnew = newplayer == 0;

   if(isnew) {
      if( newplayer = find_player(name) ) {
         set_this_player( newplayer );
         write( "\nYou ("+object_name(newplayer)+") were ousted as if link dead!\n");
         this_player()->quit(1);
         set_this_player( this_object() );
      }
      newplayer = clone_object("obj/player");
      if( level < ADMLEVEL )
         seteuid( name );
      else
         seteuid( MASTER_UID );
      export_uid( newplayer );
      newplayer->load_self(name);
   }
   exec(newplayer, this_object());
   if( want_invis )
      newplayer->set_invis( 1 );
   else if( want_vis )
      newplayer->set_invis( 0 );
   if(!password && isnew)
     newplayer->new_password(1);
   else
     newplayer->enter_world(!isnew);

   newplayer->client_init( client_code, client_ver );

   destruct(this_object());
}
