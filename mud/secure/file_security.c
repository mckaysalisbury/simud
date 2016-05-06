// This object gives a rough idea of how our file security is laid
// out. If you want to change how accessing files works, you should edit
// here.

#include <const.h>
#include <file_security.h>

#pragma strong_types

/** This file was first commented before Doxygen, so the
 * documentation might be a little strange in spots.
 * It could use some updates. Read any
 * help files/docs you can find on MUD file security and
 * it will help this code make sense. The following is
 * some notes from development, trying to decide how things
 * would work:
 *
 * Each directory needs to have:
 *    - The groups that have full access to that directory.
 *    - Groups that may create new files in that directory
 * (they may only edit the files they own).
 *    - Groups that may read files in a directory.
 * Each file needs to have:
 *    - An owner--name of the group that created it.
 * Each file accessor must have:
 *    - A list of groups of which they are a member.
 *       (their default group is their UID,
 *        additional groups are kept here).
 *    - Access is always granted if *any* of the
 *    groups have access to a file.
 * If you do not have an entry in 'groups', then you
 * are a member of only the groups 'all' and the group
 * with your name. If you do have an entry (which is
 * an array of string) then you add all those entries
 * to your uid list.
 *
 * The "owner" of a directory is the first person
 * given on the allow_modify list for the directory.
 *
 * file_owner is a list of exceptions to the file ownership
 * rules. It is assumed that a file belongs to the directory
 * owner, unless that file (full path as a key) has an
 * entry in this mapping. The only circumstance for adding
 * entries (short of attribute changes) is when a person
 * with allow_create access to a directory makes a file
 * there. That file then belongs to them, and they may
 * modify it later. In directories to which something only
 * has allow_create access, they may only modify those
 * files which they own.
 *
 * Note: keys for directories all end in "/", so when passing
 * in directories, append a "/" to their name.
 *
 */

/* A quicky macro. It should consist of code to make sure that whatever
 * object called this function is trusted to modify file access data.
 */
#define SECUREME if( getuid(previous_object()) != MASTER_UID ) return 0;

mapping groups, ///< groups is indexed by uid
        allow_minimal_read, ///< allow_'s are indexed by directory (FULL PATH).
        allow_normal_read, ///< allow_'s are indexed by directory (FULL PATH).
        allow_create, ///< allow_'s are indexed by directory (FULL PATH).
        allow_modify, ///< allow_'s are indexed by directory (FULL PATH).
        file_owner; ///< file_owner is indexed by file name (FULL PATH).

/// Adds a .o onto the end... This is the file where all the data is kept.
#define FILE_SECURITY_DATA "/secure/file_security"

/// A list of UID's used as a temporary variable in some functions.
static string *uid_list;

/** Used to backup the file security data to disk. All modifications of the
 * file security information call this immediately, so you don't have people
 * "undoing" changes by calling load_self().
 */
void save_self() {
   save_object( FILE_SECURITY_DATA );
}

/// Loads the file security data from disk.
void load_self() {
   write("Loading file_security object...\n");
   restore_object( FILE_SECURITY_DATA );
}

/** Initializes all the mappings to default (fairly useless) settings, and
 * loads the object. If there is no file security information to load, you
 * will start with the default settings. You should go through your MUD
 * then and setup things how you want them.
 */
void create() {
   groups = ([]);
   allow_minimal_read = ([ "/":({ "all" }), "/secure/":({ "root" }) ]);
   allow_normal_read = ([ "/":({ "all" }), "/secure/":({ "root" }) ]);
   allow_create = ([ "/players/":({ "all" }), "/":({ MASTER_UID }) ]);
   allow_modify = ([ "/":({ MASTER_UID }) ]);
   file_owner = ([]);
   load_self();
   call_out( "cleanup_tick", CLEANUP_TIME );
}

void cleanup_tick() {
   string key;

   call_out( "cleanup_tick", CLEANUP_TIME );

   foreach( key : allow_minimal_read )
   if( file_size(key) != -2 ) {
      log_file( "files", "allow_minimal_read: Deleted "+key+"\n");
      m_delete( allow_minimal_read, key );
   }

   foreach( key : allow_normal_read )
   if( file_size(key) != -2 ) {
      log_file( "files", "allow_normal_read: Deleted "+key+"\n");
      m_delete( allow_normal_read, key );
   }

   foreach( key : allow_create )
   if( file_size(key) != -2 ) {
      log_file( "files", "allow_create: Deleted "+key+"\n");
      m_delete( allow_create, key );
   }

   foreach( key : allow_modify )
   if( !legal_filename(key) || file_size(key) != -2 ) {
      log_file( "files", "allow_modify: Deleted "+key+"\n");
      m_delete( allow_modify, key );
   }

   foreach( key : file_owner )
   if( !file_exists(key) ) {
      log_file( "files", "file_owner: Deleted "+key+"\n");
      m_delete( file_owner, key );
   }

   save_self();
}

/// Returns the subdirectory of \a path. Used internally.
string get_subdir( string path ) {
   int i;

   if( path == "/" )
      return 0;

   for( i = strlen(path) - 1; i; )
   if( path[--i] == '/' ) {
      path = path[0..i];
      return path;
   }

   return 0;
}

/** Pass in the path of a *directory* which you are curious about,
 * and the type of requests that you are interested in, and this function
 * will return an array of uid's which have access to that directory.
 * If that directory has no uid's which are set for a given request
 * type, then 0 will be returned. The directory path must end in "/"
 *
 * See file_security.h for information on request types.
 */
string *query_dir_groups( string path, int req_type ) {
   mapping x;

   SECUREME

   if( req_type == REQ_MINIMAL_READ )
      x = allow_minimal_read;
   if( req_type == REQ_READ )
      x = allow_normal_read;
   if( req_type == REQ_CREATE )
      x = allow_create;
   if( req_type == REQ_MODIFY )
      x = allow_modify;

   while( path && !x[path] ) path = get_subdir(path);

   if( !path ) return 0;
   return x[path];
}

/** Uses the rules above. Returns a uid if \a req_type access is allowed, false
 * if it is not. Note that ban_list is an array of regular expressions. This function does
 * not use previous_object for the caller; functions like valid_read which
 * actually matter should. The uid which is returned is the one by which
 * the user is allowed to access the file; this is useful in assigning ownership.
 *
 * See file_security.h for information on request types.
 */
string query_access( string path, string uid, int req_type ) {
   string curr_uid, *allow_list;

   uid_list = ({ uid, "all" });
   if( member(groups, uid) ) uid_list += groups[uid];

// A diagnostic for file ownership. Probably delete it soon.
/*
   if( member(file_owner, path) ) {
      write("query_access: path='"+path+"' is registered.\n");
      if( member(uid_list, file_owner[path]) >= 0 )
         write("This UID list matches.\n");
      else
         write("This UID list does not match.\n");
   }
   else
      write("query_access: path='"+path+"' is not a member of file_owner\n");
*/

   if( member(file_owner, path) ) {
      if( member(uid_list, file_owner[path]) >= 0 )
         return file_owner[path];
      else if( req_type == REQ_CREATE || req_type == REQ_MODIFY )
         return 0;
   }

   // Now set rules according to read type.
   if( req_type == REQ_MINIMAL_READ ) {
      allow_list = query_dir_groups(path, REQ_MINIMAL_READ);

      if( allow_list )
      foreach( curr_uid : uid_list )
         // If this path is on the minimal read allowance
         // for your uid set...
         if( member(allow_list, curr_uid) >= 0 ) return curr_uid;
   }
   if( req_type <= REQ_READ ) {
      allow_list = query_dir_groups(path, REQ_READ);

      if( allow_list )
      foreach( curr_uid : uid_list ) {
//         write("Here is the offending code: allow_list = "+to_string(allow_list)+", curr_uid = "+to_string(curr_uid)+"\n");
         if( member(allow_list, curr_uid) >= 0 ) return curr_uid;
      }
   }
   if( req_type == REQ_CREATE ) {
      allow_list = query_dir_groups(path, REQ_CREATE);

      if( allow_list )
      foreach( curr_uid : uid_list )
         if( member(allow_list, curr_uid) >= 0 ) return curr_uid;
   }
   if( req_type == REQ_READ || req_type == REQ_CREATE || req_type == REQ_MODIFY ) {
      allow_list = query_dir_groups(path, REQ_MODIFY);

      if( allow_list )
      foreach( curr_uid : uid_list )
         if( member(allow_list, curr_uid) >= 0 ) return curr_uid;
   }

   return 0;
}

/** Adds a group to a base UID. For example, if I have the
 * base UID of "bob" and you call add_group_uid("bob", "world"),
 * it would allow me (bob) to access all things which the
 * "world" uid has access to. Returns true on successful.
 */
int add_group_uid( string uid, string group ) {

   SECUREME

   if( !stringp(group) || !stringp(uid) ) return 0;
   if( group == "" || uid == "" ) return 0;

   if( groups[uid] ) {
      if( member( groups[uid], group ) < 0 )
      groups[uid] += ({ group });
   }
   else groups[uid] = ({ group });

   save_self();

   return 1;
}

/** Inverse of add_group_uid, removes access to a group for
 * a given uid. Returns 1 if the uid was removed successfully,
 * 0 on error or if the uid didn't have access anyway.
 */
int remove_group_uid( string uid, string group ) {
   string *x;
   int i;

   SECUREME

   if( !stringp(group) || !stringp(uid) || group == "" || uid == "" ) return 0;

   x = groups[uid];
   if( !x ) return 0;

   i = member( x, group );
   if( i < 0 ) return 0;

   groups[uid] = array_delete( x, i );
   if( sizeof(groups[uid]) == 0 ) groups = m_delete( groups, uid );

   save_self();

   return 1;
}

/** \return All groups to which \a uid has access, or 0 if no
 * special groups have been added. Note that the implied groups
 * \a uid and \a all are not included in the returned array.
 */
string *query_group_uid( string uid ) {
   SECUREME

   return groups[uid];
}

/** Used to convert a string specifying request type
 * into the corresponding integer constant. Valid values for
 * \a str are view, read, create, or modify.
 */
int to_req_type( string str ) {
   if( str == "view" ) return REQ_MINIMAL_READ;
   if( str == "read" ) return REQ_READ;
   if( str == "create" ) return REQ_CREATE;
   if( str == "modify" ) return REQ_MODIFY;
   return 0;
}

/** Add a group to the list that may access a directory in
 * req_type style. Returns 0 if req_type is invalid.
 */
int add_dir_group( string path, string group, int req_type ) {
   mapping x;
   string *y;

   SECUREME

   // The path must have a '/' on the end.
   if( path[<1] != '/' ) return 0;

   if( req_type == REQ_MINIMAL_READ )
      x = allow_minimal_read;
   if( req_type == REQ_READ )
      x = allow_normal_read;
   if( req_type == REQ_CREATE )
      x = allow_create;
   if( req_type == REQ_MODIFY )
      x = allow_modify;

   // Ought never to happen
   if( !x )
      raise_error("add_dir_group failed -- mapping not allocated.");

   y = query_dir_groups( path, req_type );

   if( member(y, group) < 0 )
      x[path] = y + ({ group });

   save_self();

   return 1;
}

/** Reverses the effect of add_dir_group.
 * \return 1 if successful, 0 if not.
 */
int remove_dir_group( string path, string group, int req_type ) {
   mapping x;
   string *y;
   int i;

   SECUREME

   if( path[<1] != '/' ) return 0;

   if( req_type == REQ_MINIMAL_READ )
      x = allow_minimal_read;
   if( req_type == REQ_READ )
      x = allow_normal_read;
   if( req_type == REQ_CREATE )
      x = allow_create;
   if( req_type == REQ_MODIFY )
      x = allow_modify;

   // Ought never to happen
   if( !x )
      raise_error("add_dir_group failed -- mapping not allocated.");

   y = query_dir_groups( path, req_type );

   i = member(y, group);
   // Can't remove a group that isn't there.
   if( i < 0 ) return 0;
   x[path] = array_delete( y, i );

   if( sizeof(x[path]) == 0 ) x[path] = 0;

   save_self();

   return 1;
}

/** Sets the owner of a directory. This is basically
 * a special way of setting the "modify" list, since
 * the first entry on the modify list is
 * considered the owner.
 *
 * \a path must end in a "/"
 */
int set_dir_owner( string path, string owner ) {
   string *x;

   SECUREME

   // The usual.
   if( path[<1] != '/' ) return 0;

   x = query_dir_groups( path, REQ_MODIFY );

   // Should do nothing if owner not already in x.
   x = array_delete( x, member(x, owner) );

   // First entry in allow_modify array is considered the owner.
   allow_modify[path] = ({ owner }) + x;

   save_self();

   return 1;
}

/** \return The owner of the directory specified by
 * \a path.
 */
string query_dir_owner( string path ) {
//   write("Trying to get dir owner...\n");
   if( path[0] != '/' ) return 0;

   // The first entry in the allow_modify array is the dir owner.
   while( path && !allow_modify[path] ) {
      path = get_subdir( path );
   }
   if( !path ) return 0;
   return allow_modify[path][0];
}

/** \return The owner of a file. This function will return 0 if there
 * is no owner set for the file, but this should be vanishingly rare
 * (it indicates an ownerless directory, which although not kosher
 * is possible -- if directories are created from an outside shell
 * for example, and recursing back finds nothing).
 */
string query_file_owner( string path ) {
   int i;
   string owner;

//   write( "In query_file_owner("+path+") ... path[0] = " + as_lpc(path[0..0]) + " (" + as_lpc(path[0]) + ")\n");
   if( path[0] != '/' ) {
      debug("Invalid path to query_file_owner() from " + as_string(previous_object()) + ": " + as_lpc(path) );
      return 0;
   }
//   write( "q_f_o was non-zero...\n");
   // First check if file has specified owner.
   owner = file_owner[path];
//   write( "Checked for owner: " + as_lpc(owner) + "\n" );

   // If not get the owner of the directory that the file is in.
   if( !owner ) {
      for( i = strlen(path); i; )
      if( path[--i] == '/' ) {
         path = path[0..i];
         break;
      }
      owner = query_dir_owner(path);
   }
//   write( "Checked for dir owner (if owner was 0, might change): " + as_lpc(owner) + "\n" );

   return owner;
}

/** Sets the owner of a file. You must pass in the full path to
 * the file. This will work even if the file doesn't exist.
 * Later on, however, there should be a daemon that cleans up
 * file owner entries for nonexistent files, so don't try
 * to use this to hide information or similar.
 */
int set_file_owner( string file, string owner ) {
   SECUREME

   if( !file || !owner ) return 0;

   // Speculatively set owner to null, allowing this to
   // be an un-set as well.
   file_owner = m_delete( file_owner, file );

   // Nothing to do.
   if( query_file_owner(file) == owner ) return 1;

   file_owner[ file ] = owner;

   save_self();

   return 1;
}

/** \return One of the file access mappings. It would
 * generally be unkosher to modify the mapping returned (that
 * would change file access information), but that is a
 * valid way to do it; if you do this, however, see
 * save_self().
 */
mapping query_access_table( int req_type ) {
   SECUREME

   if( req_type == REQ_MINIMAL_READ )
      return allow_minimal_read;
   if( req_type == REQ_READ )
      return allow_normal_read;
   if( req_type == REQ_CREATE )
      return allow_create;
   if( req_type == REQ_MODIFY )
      return allow_modify;
   return 0;
}

/** \return True if \a caller is allowed to modify variables
 * in \a callee, false otherwise.
 */
int query_allow_set( object caller, object callee ) {
   string callee_uid, caller_uid, *uid_list;

//   write("caller="+to_string(caller)+"\n");
   caller_uid = getuid(caller);
//   write("callee="+to_string(callee)+"\n");
   callee_uid = getuid(callee);

   if( callee_uid == "all" || callee_uid == caller_uid || caller_uid == MASTER_UID ) return 1;
   uid_list = groups[caller_uid];
   if( uid_list && member(uid_list, callee_uid) >= 0 ) return 1;
   return 0;
}
