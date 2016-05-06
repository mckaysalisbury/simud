#include <object.h>
#include <const.h>
#include <textdraw.h>

#define DAEMON ("/daemon/chat")
#define I3DAEMON ("/secure/sockets/i3/intermud3")

/// To be documented.

int muted;
int query_muted() { return muted; }

/// Repair a player's chat line subscriptions
int do_fix(int silent) {
  string *chans = this_player()->query_env_var("channels");
  chans = sort_array(m_indices(mkmapping(chans)), #'>);
  if( sizeof(chans) != sizeof(this_player()->query_env_var("channels")) ) {
    this_player()->set_env_var("channels", chans);
    msg("~CACTYour chat lines have been recalibrated.~CDEF");
  } else if( !silent ) {
    msg("Your channel settings are just fine.");
  }
  return 1;
}

/** Doesn't inherit 'desc' for simple efficiency, so it must define its own id() lfun.
 * Returns true if it is passed the string 'chat'. This may be changed later, it
 * was only done for wiz convenience.
 */
object id(string str) { return str=="chat"?this_object():0; }

string query_name() { return "chat"; }

/// Initialize the chat object, and queries the channel variable from the player.
void create() {
  ::create();
  muted = 0;
  if(!this_player()) return;
  if(!this_player()->query_env_var("channels")) {
 	 this_player()->add_env_var("channels", "chat");
 	 this_player()->add_env_var("channels", "newbie");
  }
  do_fix(1);
}

/** Starts listening on a new channel, and resets this chat object. \a chan may be any string.
 * \return True on success, false otherwise (channel already added).
 */
int add_channel(string chan, int silent) {
  if (DAEMON->enter(chan,this_player(),silent))
     this_player()->add_env_var("channels", chan);
  else
     return 0;
  if(environment(this_object()))
     move_object(this_object(), environment(this_object()));
  return 1;
}

/** Stops listening to a channel, and resets this chat object. \a chan may be any string.
 * \return True on success, false otherwise (no such channel).
 */
int remove_channel(string chan) {
   DAEMON->exit(chan,this_player());
   this_player()->remove_env_var_item("channels", chan);
   if (environment(this_object()))
      move_object(this_object(), environment(this_object()));
   return 1;
}

/** Called whenever the chat object is reset. This is done every time channel settings
 * are changed, and when the player receives the object. Adds the channel, mute, and
 * chat commands to the player (one chat command for each channel).
 */
void init() {
  int i;
  string * channels;

  channels = this_player()->query_env_var("channels");
  if( !pointerp(channels) ) channels = ({});
  for (i = 0; i < sizeof(channels); i++ ) {
     add_action( "chat", channels[i] );
     add_action( "hist", channels[i]+"hist" );
     add_action( "cmd", channels[i]+"cmd" );
  }
  add_action( "channel", "channel" );
  add_action( "mute", "mute" );
  add_action( "do_fix", "fixchat" );
}

/** Allows the player to mute all the channels they are currently listening to.
 */
int mute() {
  if(!muted) {
    muted = 1;
    msg("You have muted all channels.");
    return 1;
  }
  muted = 0;
  msg("You open your ears to the world.");
  return 1;
}

/** Provides the 'channel' command (verb). With no parameters, lists current
 * channels. Otherwise, this lets you add or drop channels.
 */
int channel(string comm) {
  string sw, chan, string1;
  int j, cols, i;
  string * channels;

  channels = this_player()->query_env_var("channels");
  if(!channels) channels = ({});

  if(!comm || strlen(comm) == 0) {
    // show current channels
    msg("~CBRTYou are in the following channels:");
    string1="~CDEF";

    cols = (this_player()->query_term_width()-2) / 15;
    for(i=0;i<sizeof(channels);i++)
      string1 += pad(capitalize(channels[i]), 14, 2) + (((i+1)%cols == 0)?"\n":"");

    string1 = process_string(string1, 0);
    msg_bare(string1+"\n");

	// show admin a list of all channels in use
	if( this_player()->query_level() >= ADMLEVEL ) {
		mapping chanmap = DAEMON->query_list();
		channels = ({});
		foreach( chan : m_indices(chanmap) )
			channels += ({ chan + "("+sizeof(chanmap[chan])+")" });
		msg("~CBRTAll channels in game: ~CDEF");
		write(comma_list(sort_array(channels,#'>)) + "\n");
	}
	
    return 1;
  }

  if(comm[0]!='+' && comm[0]!='-') {
    msg("==> [Format] channel [<+/-><channel>]");
    return 1;
  }

  if(sscanf(comm, "-%s", chan) == 1 ) sw = "-";
   else if(sscanf(comm, "+%s", chan) == 1) sw = "+";

  chan = trim(chan);
  if(!chan || chan=="") {
     msg("==> [Format] channel [<+/-><channel>]");
     return 1;
  }

  if(sw=="-") {
    if(!remove_channel(chan)) {
      msg("==> [Error] Could not remove that channel! It may not exist.");
      return 1;
    }
    msg("Channel "+chan+" removed.");
  } else
  if(sw=="+") {
    if( file_exists("/bin/" + chan + ".c") ||
      file_exists("/bin/wiz/" + chan + ".c") ||
      file_exists("/bin/soul/" + chan + ".c") ||
      member( query_actions(this_player(), 1), chan) >= 0 ) {
      notify_fail("Adding that channel failed! (Perhaps you're already in it?)\n");
      return 0;
    }
    if(!add_channel(chan, 0)) {
      msg("==> [Error] Failed to add channel! It may already exist.");
      return 1;
    }
    msg("Channel "+chan+" added.");
  } else {
    msg("==> [Format] channel [<+/-><channel>]");
  }
  return 1;
}

/** Displays chat line history
 */
varargs int hist(string chan) {
   mapping map = DAEMON->query_hist();
   if (!chan)
      chan = query_verb()[..<5];
   if (!member(map,chan)) {
      msg("No such channel '"+chan+"'.");
   } else if (sizeof(map[chan]) == 0) {
      msg("No history for channel '"+chan+"'.");
   } else {
      string buf = "~CTIT"+capitalize(chan)+" History~CDEF\n";
      int x;
      mixed line;
      for (x = 0; x < strlen(chan) + 8; x++)
         buf += "-";
      msg(buf);
      foreach (line : map[chan]) {
         string color;
         if (line[0] == this_player()->query_name())
            color = "~CDRK";
         else
            color = "~CACT";
         msg("(~CCOM"+line[1]+"~CDEF) "+color+""+line[2]+"~CDEF",11);
      }
   }
   return 1;
}

/** Broadcasts a chat message.
 */
int chat(string mssg) {
  string *words;
  int uses_target;
  string chan = query_verb();

  if (chan[0..4] == "debug" || chan == "error")
     return 0;

  if(muted) {
     msg("You are muted. Go away.");
     return 1;
  }

  if(!mssg || mssg == "") {
     write("Use the chat commands like this: '"+chan+" message' or '"+chan+" :emote'\n");
     return 1;
  }

  if ( trim(mssg) == "-h" ) {
     hist(query_verb());
     return 1;
  }

  if (chan == "icode" || chan == "igossip") {
     chan = "imud_" + chan[1..];
     if (mssg[0] == ':' || mssg[0] == ';')
        I3DAEMON->send_channel_emote(this_player()->query_cap_name(),
                                     chan, "$N " + mssg);
     else
        I3DAEMON->send_channel_message(this_player()->query_cap_name(),
                                       chan, mssg);
  } else {
     if ( mssg[..0] == ":" )
        mssg = recolor(mssg, "~CEMO");
     else
        mssg = recolor(mssg);
     DAEMON->chat(chan, this_player()->query_name(), mssg);
  }
  return 1;
}

int owner_check(object who, string chan) {
	string *owners = DAEMON->query_owner()[chan];
	if( pointerp(owners) && member(owners, who->query_name()) != -1 )
		return 1;
	if( who->query_level() >= ADMLEVEL )
		return 1;
	return notify_fail("You don't have permission to do that.");
}

int cmd(string args) {
	string chan = query_verb()[..<4];
	string cmd, tmp, buf = "";
	mapping data;
	if( args ) {
		sscanf( args,"%s %s",cmd,args );
		if( !cmd ) {
			cmd = args;
			args = 0;
		}
	}
	debug( "chan = "+chan+", cmd = "+cmd+", args = "+args, "chat");
	switch( cmd ) {
		case "hist":
			return hist(chan);
		
		case "info":
			msg("~CTITChannel information for '"+chan+"'~CDEF");
			msg("Flags:   "+DAEMON->query_perm_str(chan));
			msg("Owners:  "+comma_list(DAEMON->query_owner()[chan]), 9);
			msg("Invites: "+comma_list(DAEMON->query_invite()[chan]), 9);
			msg("Voice:   "+comma_list(DAEMON->query_voice()[chan]), 9);

			data = DAEMON->query_list();
			buf = comma_list(sort_array(filter(data[chan],#'find_player),#'>));
			msg("Users:   "+buf, 9);
			break;

		// same subsystem for changing user lists
		case "owner":
		case "invite":
		case "voice":
			if( owner_check(this_player(), chan) ) {
				msg("List administration isn't working yet.");
			} else
				return 0;
			break;

		case "color":
			if( owner_check(this_player(), chan) ) {
				msg("List administration isn't working yet.");
			} else
				return 0;
			break;
				
		case "delete":
			if( owner_check(this_player(), chan) ) {
				msg("You have deleted the channel '"+chan+"'");
				DAEMON->del_chan( chan );
			} else
				return 0;
			break;
				
		case "help":
		default:
			msg("Chat commands:\n"+
				"  ~CCOMinfo~CDEF - information on the channel\n"+
				"  ~CCOMhist~CDEF - channel history\n");
	}
	return 1;
}

void move( object dest ) {
   if( present("chat", dest) ) call_out("kaboom", 0);
   if( !environment(this_object()) || !environment(this_object())->query_is_player() )
      move_object( this_object(), dest );
   // Will not move if environment is already a player.
}

void kaboom() {
   object ob;
   int nchat;

   nchat = 0;
   if( environment() )
   foreach( ob : all_inventory(environment()) ) {
      if( ob->id("chat") ) nchat++;
      if( nchat > 1 ) {
         destruct( this_object() );
         return;
      }
   }
   else
      destruct(this_object());
}
