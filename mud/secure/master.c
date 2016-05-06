#include "/include/sys/driver_hook.h"
#include "/include/file_security.h"
#include "/include/const.h"
#include "/include/debug_info.h"

/// \todo Document this object.

mapping hold_var;

#define SIMUL_EFUN_FILE "/secure/simul_efun"
#define SPARE_SIMUL_EFUN_FILE "/secure/spare_simul_efun"

void DEBUG( string x ) {
   object ob;

   foreach( ob : users() ) {
      if( ob->query_level() >= WIZLEVEL )
         tell_object(ob, "[/secure/master] " + to_string(x) + "\n");
   }
}

//#define DEBUG(x)	call_other( SIMUL_EFUN_FILE, "debug", x )
//#define DEBUG(x)         call_other( "/obj/chat", "do_msg", "~CDEF[~CLABDebug~CDEF] " + x, "debug" )
//#define DEBUG(x)         write("Temp debug: "+x+"\n")

string *epilog( int eflag );

// alternative to using chat daemon
void error_msg(string errmsg) {
   object who;
   string *chanlist;
   foreach ( who : users() - ({ this_player() }) )
      if( pointerp(chanlist = who->query_env_var("channels")) )
         if (member(chanlist, "error") != -1) {
            if( find_object("/daemon/ansi") && find_object(SIMUL_EFUN_FILE) )
               call_other( SIMUL_EFUN_FILE, "msg_object", who, "~[010[~CWRNError~[010]~CDEF "+errmsg, 8 );
            else
               tell_object( who, "[Error] " + errmsg );
         }
}

void log_error(string file, string err) {
   // Strip \n's
   while( err[<1] == '\n' ) err = err[0..<2];

   // Message to this_player
   if( this_player() && this_player()->query_level() >= WIZLEVEL ) {
      write( "\n"+ctime(time()) + " from file " + file + ": " + err + "\n" /*+
         debug_info(DINFO_TRACE, 1)*/ );
   } else if ( this_player() )
      tell_object( this_player(), "\nYou feel a non-specific disturbance in the force.\n" );

   // Message to logs
//   if( find_object(SIMUL_EFUN_FILE) )
//      call_other( SIMUL_EFUN_FILE, "log_file", ERRORS_LOG, ctime(time()) + " from file " + file + ": " + err + "\n" +
//         /*debug_info(DINFO_TRACE, 1) +*/ "\n" );
// /hedwig.debug.log works better anyway...

   // Message to errors channel
   catch( error_msg(err) );

   // Old way of doing this:
   // if( find_object("/obj/chat") )
   //    call_other("/obj/chat", "do_msg", "~CDEF[~CLABError~CDEF] "+err, "error" );
}

void runtime_error(string err, string prg, string curobj, int line) {
   while( err[<1] == '\n' ) err = err[0..<2];

   // Message to this_player
   if( this_player() && this_player()->query_level() >= WIZLEVEL )
      write( "\n" + ctime(time()) + " RUNTIME: prg=/"+prg+", curobj="+curobj+", line="+line+", err=" + err + "\n" );
   else if ( this_player() )
      write( "\nYou feel a non-specific disturbance in the force.\n" );

   // Message to logs
   if( find_object(SIMUL_EFUN_FILE) )
      call_other( SIMUL_EFUN_FILE, "log_file", ERRORS_LOG, ctime(time()) +
         " RUNTIME: prg=/"+prg+", curobj="+curobj+", line="+line+", err=" + err + "\n" +
         /*debug_info(DINFO_TRACE, 1) +*/ "\n" );

   // Message to errors channel (the only to use ANSI, so the most risky, which is why the catch).
   catch( error_msg(curobj+" in /"+prg+" line "+line+": "+err) );

   // Old way of doing this:
   // if( find_object("/obj/chat") )
   //   call_other("/obj/chat", "do_msg", "~CDEF[~CLABRuntime Error~CDEF] "+curobj+" in /"+prg+" line "+line+": "+err, "error");
}

get_master_uid() {
   return "root";
}

get_bb_uid() {
   return "\nBB_UID_NOT_USED\n";
}

mixed creator_file( mixed ob ) {
   string fname, owner;

   if( stringp(ob) ) {
      if( ob[0] != '/' ) ob = "/" + ob;
   }
   if( !ob ) return 0;
   if(objectp(ob)) {
      fname = load_name(ob) + ".c";
   }
   else fname = ob;
   if( fname[<2..<1] != ".c" ) fname += ".c";

   if( fname[0..strlen(AUTO_ROOT_PREFIX)-1] == AUTO_ROOT_PREFIX ) {
      return MASTER_UID;
   }

   // A special UID is given to players because it will almost immediately
   // be replaced by something else. Once the player object has loaded,
   // the object that cloned it (which MUST be a "root" object) should
   // set its euid to the player's name and export it to the player object.
   // Note that the player is given an EUID of 0 for now.
   if( fname[0..strlen(PLAYER_OBJ)-1] == PLAYER_OBJ ) {
      return ({ "player_obj\n", 0 });
   }

   // Fetch the object's file owner.
   owner = "/secure/file_security"->query_file_owner(fname);
   if( !owner ) {
      owner = "temp";
   }

   return owner;
}

void stamp_load( mixed file ) {
   if( !stringp(file) ) return;
   if( file[0] != '/' ) file = "/" + file;
   if( file[<2..<1] == ".c" ) file = file[..<3];
   if( file == "/daemon/uptodate" || file[..strlen(AUTO_ROOT_PREFIX)-1] == AUTO_ROOT_PREFIX) return;
      // Stuff that isn't tracked

   if( find_object("/daemon/uptodate") )
      "/daemon/uptodate"->on_load( file );
}

int valid_seteuid( object ob, string newid ) {
//   write("valid_seteuid("+object_name(ob)+","+newid+")\n");
   // Always allowed
   if( !newid ) return 1;

   if( getuid(ob) == MASTER_UID ) return 1;
   if( creator_file(ob) == newid ) return 1;
   if( getuid(ob) == newid ) return 1;
   if( getuid(ob) == FLEX_UID && getuid(this_player()) == newid ) return 1;

   write("valid_seteuid(" + (objectp(ob)?object_name(ob):"<null>") + ", " + newid + ") denied!\n");
   // Sorry, no can do!
   return 0;
}

int valid_exec( string name, object dest, object src ) {
//   write("valid_exec("+name+","+object_name(dest)+","+object_name(src)+")\n");
   if( getuid(find_object(name)) == MASTER_UID ) // Object requesting the exec must be root.
      return 1;
   else
      return 0;
}

/*
 * Define where the '#include' statement is supposed to search for files.
 * "." will automatically be searched first, followed in order as given
 * below. The path should contain a '%s', which will be replaced by the file
 * searched for.
 */
/*
string *define_include_dirs() {
   write("define_include_dirs\n");
   return ({"include/%s"});
}
*/

/*
 * This function is called every time a player connects.
 * input_to() can't be called from here.
 */
object connect() {
    object ob;
    string ret;

#ifdef __TLS__
	if( query_mud_port() == SSL_PORT && tls_available() ) {
		return clone_object("/secure/tls_login");
	}
#endif

    write("#$#mcp version: 2.1 to: 2.1\n");
    write("Welcome to "+MUD_TITLE+" running "+VERSION+".\n");

#if 0
    ret = (string)catch(ob = clone_object("secure/login"));
#else
    ob = clone_object("secure/login");
#endif

    write("\n");
    if (ret) {
	write(ret + "\n");
	return 0;
    }
#ifdef MUDWHO
    mudwho_info[ob] =
      sprintf("A\t%.20s\t%.20s\t%.20s\t%.20s\t%:010d\t0\tlogin",
        MUDWHO_NAME, MUDWHO_PASSWORD, MUDWHO_NAME,
        explode(object_name(ob), "#")[<1] + "@" MUDWHO_NAME, time()
      )
    ;
#endif /* MUDWHO */
    return ob;
}

/*
 * This function is called when a player's connection is lost.
 */
void disconnect( object ob ) {
   // This is necessary, otherwise valid_write won't be told [correctly] who
   // is attempting to do the write operation (so that the player can save
   // their data).
   set_this_player( ob );
   ob->quit(3);
}

// I don't know what this does.
#define BACKBONE_WIZINFO_SIZE 3
#ifndef NATIVE_MODE
#define NATIVE_MODE 1
#endif
#ifndef AMYLAAR321
#define AMYLAAR321 1
#endif

/** Handles certain kinds of security issues.
 * \todo Finish this function to make it work completely.
 */
int privilege_violation(string op, mixed who, mixed arg3, mixed arg4) {
//   DEBUG("privilege_violation("+op+", "+object_name(who)+", ...);\n");
   switch( op ) {
      case "input_to": if( object_name(who)[0..10] == "/obj/player" ) return 1; return 0;
      case "wizlist_info": return 1;
      case "execute_command": if( getuid(who) == MASTER_UID || who == arg3 ) return 1; return 0;
	  case "attach_erq_demon":	// added Feb 12, '05 -Al
	  case "bind_lambda":		// also added Feb 12, for xerq support
      case "erq": if( getuid(who) == MASTER_UID ) return 1; return 0;
      case "set_this_object": if( getuid(who) == MASTER_UID ) return 1; return 0;
      case "limited": if( getuid(who) == MASTER_UID || getuid(who) == "world" ) return 1; return 0;
      case "nomask simul_efun":
         if( objectp(who) && getuid(who) == MASTER_UID ) return 1;
         if( stringp(who) && (who[..6] == "secure/" || who[..7] == "/secure/") ) return 1;
         return 0;
      case "call_out_info": return 1;
	  // added for 3.3.659 - Al [Jan 26, '05]
	  case "mysql": return object_name(who)[..7] == "/secure/";
   }
   DEBUG("Unsupported privilege request type: "+op);
   return 0;
}

string used_uid;

string query_used_uid() {
   return used_uid;
}

mixed valid_write(string path, string eff_user, string call_fun, object caller)
{
//   return 1;

   if( path[0] != '/' ) path = "/" + path;
   if( (eff_user == FLEX_UID || !eff_user) && this_player() ) eff_user = getuid( this_player() );

//   DEBUG("master->valid_write("+path+","+eff_user+","+call_fun+","+object_name(caller)+"); prev is "+object_name(previous_object()));

   // The Admin back door here...
   if( eff_user == MASTER_UID ) return 1;

   // If the object tries to save itself in the "/data/" directory, which is used
   // by the saveload stuff, then it gets a shortcut security check. This is
   // partially to get them done faster, and partially to avoid having to make
   // the file_security rules more complex.
   if( path[0..5] == "/data/" ) {
//      DEBUG("Checking as a /data/ object.");
      // If they're not a saveload object, automatically deny any access
      // to this directory.
      if( member(inherit_list(caller), "/inherit/saveload.c") < 0 ) {
//         DEBUG("Inherit check fails");
         return 0;
      }
      // Check to make sure that their Unique ID matches the file they are trying to
      // write. If so, all is ok.
      if( "/secure/odin"->get_save_file_name(caller->query_unique_id()) == path ) return path;
//      DEBUG("File match fails");
      // Nope, this is illegal (not a call from saveload, then, but from another function
      // inside the saveload object. Saveload will not make illegal calls, barring bugs).
      return 0;
   }

   // For save & restore object, the path given is NOT the actual path.
   // This is stupid, but there you go. You have to add a .o to get
   // the path of the file.
   if( call_fun == "save_object" ) path += ".o";

//   DEBUG("valid_write: Not root, eff_user is "+eff_user+", path is "+path);

   // Check as REQ_MODIFY first, so that users with modify access can
   // use the native owner UID.
   used_uid = "/secure/file_security"->query_access( path, eff_user, REQ_MODIFY );
   // If that fails, but the file doesn't exist, check for the lower-
   // privilege REQ_CREATE permission as well.
   if( !used_uid && file_size(path) == -1 ) {
      // Check as REQ_CREATE
      used_uid = "/secure/file_security"->query_access( path, eff_user, REQ_CREATE );
      if( used_uid )
         "/secure/file_security"->set_file_owner( path, used_uid );
   }

   if( used_uid ) return 1; else return 0;
}


mixed valid_read(string path, string eff_user, string call_fun, object caller)
{
//   return 1;

   // For legacy code to work right.
   if( path[0] != '/' ) path = "/" + path;
   if( (eff_user == FLEX_UID || !eff_user) && this_player() ) eff_user = getuid( this_player() );

//   write("master->valid_read("+path+","+eff_user+","+call_fun+","+object_name(caller)+"); prev is "+object_name(previous_object())+"\n");

   // The "Admin" back door to file access. This is actually different from what
   // we had before -- it's now based on possessing the root uid!
   if( eff_user == MASTER_UID ) return 1;

//   write("valid_read: Not root, eff_user is "+eff_user+"\n");
   if( call_fun == "restore_object" ) path += ".o";

   // File access depends on the operation desired.
   if( call_fun == "file_size" || call_fun == "get_dir" )
      used_uid = "/secure/file_security"->query_access( path, eff_user, REQ_MINIMAL_READ );
   else used_uid = "/secure/file_security"->query_access( path, eff_user, REQ_READ );

//   if( used_uid ) write("Access SHOULD be granted for "+call_fun+"\n"); else write("Access denied for "+call_fun+"\n");
   if( used_uid ) return 1; else return 0;
}


/** Stolen straight from Heaven7, to help us get up and running
 * in native mode. It's been pretty modified a bit. If you wanted
 * to trim it down to get rid of the compat mode stuff, you could,
 * but it's not really necessary.
 */
void inaugurate_master(int arg)
{
   write("Loading master object...\n");
   hold_var = ([ ]);
#if HAVE_WIZLIST == 0
    if (!arg)
        set_extra_wizinfo(0, allocate(BACKBONE_WIZINFO_SIZE));
    if (find_call_out("wiz_decay") < 0)
        call_out("wiz_decay", 3600);
#endif /* HAVE_WIZLIST */

   set_driver_hook(
      H_INCLUDE_DIRS,
      ({ "/include/" })
   );
   set_driver_hook(
      H_DEFAULT_METHOD,
      "catch_call"
   );
   set_driver_hook(
      H_AUTO_INCLUDE,
      "#include \"/include/const.h\"\n"
   );
   set_driver_hook(
      H_NOTIFY_FAIL,
      "I'm sorry, but I didn't understand that.\n");
#if defined(AMYLAAR321)
    set_driver_hook(
      H_MOVE_OBJECT0,
      unbound_lambda( ({'item, 'dest}), ({#',,
#ifdef NATIVE_MODE
     ({#'?, ({#'!=, 'item, ({#'this_object})}),
       ({#'raise_error,
         "Illegal to move other object than this_object()\n"}) }),
#endif
#ifdef COMPAT_FLAG
     ({#'&&, ({#'living, 'item}), ({#'environment, 'item}), ({#',,
       ({#'efun::set_this_player, 'item})
       ({#'call_other, ({#'environment, 'item}), "exit", 'item}),
     }) }),
#endif
     ({#'efun::set_environment, 'item, 'dest}),
     ({#'?, ({#'living, 'item}), ({#',,
       ({#'efun::set_this_player, 'item}),
       ({#'call_other, 'dest, "init"}),
       ({#'?, ({#'!=, ({#'environment, 'item}), 'dest}), ({#'return})}),
     }) }),
     ({#'=, 'others, ({#'all_inventory, 'dest}) }),
     ({#'=, ({#'[, 'others, ({#'member, 'others, 'item}) }), 0}),
     ({#'filter, 'others,
       ({#'bind_lambda,
         unbound_lambda( ({'ob, 'item}),
           ({#'?, ({#'living, 'ob}), ({#',,
          ({#'efun::set_this_player, 'ob}),
          ({#'call_other, 'item, "init"}),
           }) })
         )
       }),
       'item,
     }),
     ({#'?, ({#'living, 'item}), ({#',,
       ({#'efun::set_this_player, 'item}),
       ({#'filter_objects, 'others, "init"}),
     }) }),
     ({#'?, ({#'living, 'dest}), ({#',,
       ({#'efun::set_this_player, 'dest}),
       ({#'call_other, 'item, "init"}),
     }) }),
      }) )
    );
#ifdef COMPAT_FLAG
    set_driver_hook(
      H_LOAD_UIDS,
      unbound_lambda( ({'object_name}),
     ({#'?,
       ({#'==,
         ({#'sscanf, 'object_name, "players/%s", 'wiz_name}),
         1,
       }),
       ({#'?,
         ({#'==,
           ({#'sscanf, 'wiz_name, "%s/%s", 'start, 'trailer}),
           2,
         }),
         ({#'&&, ({#'strlen, 'start}), 'start}),
         'wiz_name
       }),
       ({#'&&,
         ({#'!=, ({#'[..], 'object_name, 0, 3}), "ftp/"}),
         ({#'!=, ({#'[..], 'object_name, 0, 4}), "open/"}),
       })
     })
      )
    );
    set_driver_hook(
      H_CLONE_UIDS,
      unbound_lambda( ({'blueprint, 'new_name}), ({
     #'||,
       ({#'creator, 'blueprint}),
       ({#'creator, ({#'previous_object})}),
       1
      }) )
    );
    set_driver_hook(H_CREATE_SUPER, "reset");
    set_driver_hook(H_CREATE_OB,    "reset");
    set_driver_hook(H_CREATE_CLONE, "reset");
#else
    set_driver_hook(
      H_LOAD_UIDS,
      unbound_lambda( ({'object_name}),
      ({ #',, ({ #'stamp_load, 'object_name }), ({ #'creator_file, 'object_name }) }) )
    );
    set_driver_hook(
      H_CLONE_UIDS,
      unbound_lambda( ({ /* object */ 'blueprint, 'new_name}),
        ({ #'creator_file, ({ #'program_name, 'blueprint }) })
      )
    );
#ifdef NATIVE_MODE
    set_driver_hook(H_CREATE_OB,    "create");
    set_driver_hook(H_CREATE_CLONE, "create");
#else
    set_driver_hook(H_CREATE_SUPER,
      unbound_lambda(0, ({#',,
       ({#'call_other, ({#'this_object}), "create"}),
       ({#'call_other, ({#'this_object}), "reset"})
     })
      )
    );
    set_driver_hook(H_CREATE_OB,
      unbound_lambda(0, ({#',,
       ({#'call_other, ({#'this_object}), "create"}),
       ({#'call_other, ({#'this_object}), "reset"})
     })
      )
    );
    set_driver_hook(H_CREATE_CLONE,
      unbound_lambda(0, ({#',,
       ({#'call_other, ({#'this_object}), "create"}),
       ({#'call_other, ({#'this_object}), "reset"})
     })
      )
    );
#endif
#endif
    set_driver_hook(H_RESET,        "reset");
    set_driver_hook(H_CLEAN_UP,     "clean_up");
//    set_driver_hook(H_MODIFY_COMMAND, ALIAS_LIST);
    set_driver_hook(H_MODIFY_COMMAND_FNAME, "modify_command");

#endif /* AMYLAAR321 */
}


/* amylaar: allow to change simul_efun_file without editing patchlevel.h */
#ifndef SIMUL_EFUN_FILE
    /* the definition should be compatible with filename(). */
#define SIMUL_EFUN_FILE "/obj/simul_efun"
#endif

#ifndef SPARE_SIMUL_EFUN_FILE
#define SPARE_SIMUL_EFUN_FILE "/obj/spare_simul_efun"
#endif
/*
 * Give a path to a simul_efun file. Observe that it is a string returned,
 * not an object. But the object has to be loaded here. Return 0 if this
 * feature isn't wanted.
 */
string get_simul_efun() {
    string fname;
    string error;

    fname = SIMUL_EFUN_FILE ;
    // Temp code trying to generate a real error
//    call_other(fname, "test_func");
    error = catch(call_other(fname, "this_function_not_exist"));
    if ( error ) {
	fname = SPARE_SIMUL_EFUN_FILE ;
	if (error = catch(call_other(fname, "??"))) {
	    shutdown();
	    return 0;
	}
    }
    return fname;
}


void move_or_destruct(object what, object to)
/* An error in this function can be very nasty. Note that unlimited recursion
 * is likely to cause errors when environments are deeply nested
 */
{
   write("move_or_destruct\n");
    if ( !catch( what->move(to, 1) ) ) return;

    /*
     * Failed to move the object. Then, it is destroyed.
     */
    destruct(what);
}

mapping nukables;

void add_nukable( object ob ) {
   if( !mappingp(nukables) )
      nukables = ([ ]);

   nukables -= ([ 0 ]);
   if( objectp(ob) )
      nukables += ([ ob ]);
}

mapping query_nukables() {
   if( !mappingp(nukables) ) nukables = ([ ]);
   return nukables;
}

mixed prepare_destruct(object ob) {
   object super, curr;
   object *inv;
   string err;
   mixed ret;

   if( !mappingp(nukables) )
      nukables = ([ ]);

   // Check if the object wants to do any cleanup work. We use the
   // function name "destructor" for this.
   if( !member(nukables, ob) && function_exists("destructor", ob) ) {
      if( catch(ret = ob->destructor( previous_object() )) )
         DEBUG( "Error in destructor: " + to_string(ob) + "; destructing anyway." );
      if( ret && ret != "" ) return ret;
   }

   if( !member(nukables, ob) && member(epilog(0), load_name(ob)) >= 0 )
      call_out("forced_reload", 0, load_name(ob) );

   if( !member(nukables, ob) && ob != find_object("/daemon/uptodate") )
      "/daemon/uptodate"->on_destruct(ob);

   // Check if object has an inventory. If not, we're done!
   if( !first_inventory(ob) ) return 0;

   inv = all_inventory( ob );
   if( inv && sizeof(inv) ) {
      super = environment( ob );
      if( !super ) super = load_object("/obj/void");
      if( !super )
         return "in /secure/master: prepare_destruct failed -- no environment, can't find void.";

       // Clean out inventory of object.
      foreach( curr : inv )
      if( !member(nukables, curr) )
         curr->move(super);
   }

   // Whatever's left, destruct it.
   while( ob && curr = first_inventory(ob) ) {
      curr->destructor( this_object() );
      destruct( curr );
   }

   return 0;
}

void forced_reload( string name ) {
   load_object( name );
}

string *epilog( int eflag ) {
   // This returns a list of objects that we always want loaded; the world,
   // the object that tracks change in time, the HTTP daemon, and the void.
   // Also have added the ansi and verb objects -- the error reporting
   // doesn't work right until they're loaded.
   return ({ "/daemon/uptodate", "/daemon/time", "/daemon/desc_vars", "/obj/void", "/daemon/ansi", "/obj/verb", "/obj/chat", "/world/world", "/secure/sockets/i3/intermud3" });
}

void preload( string file ) {
   string err;

   // Load the object specified by file.
   err = catch(load_object(file));
   if( err ) write("Problems in preloading object "+file+": "+err+"\n");
}

void set_hold_var( mixed var ) {
   hold_var[load_name(previous_object())] = var;
}

mixed query_hold_var() {
   return hold_var[load_name(previous_object())];
}

/** Currently, you're only allowed to shadow the special mirror
 * object, /obj/mirror. Root objects can also shadow anything they
 * like...
 */
int query_allow_shadow( object victim ) {
   if( load_name(victim) == "/obj/mirror" || getuid(previous_object()) == MASTER_UID ) return 1;
   return 0;
}
