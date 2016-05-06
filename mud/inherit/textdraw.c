#include <list.h>
#include <keys.h>
#include <const.h>
#include <strings.h>

/** This is a file designed to contain functions that will simplify the
 * life of anyone trying to do complex things using ANSI codes. Most of
 * these functions are layered on top of the built in ANSI support,
 * so they might not do what you want if the player has ANSI turned
 * off. You should check this with this_player()->query_ansi(), which
 * will return true if the player has ANSI turned on.
 */

/** Figures out the ~ codes to do an adjusted gotoxy.
 * Although there is no limit on the absolute coordinates
 * which are allowed by the ANSI codes for moving the cursor,
 * the tilde codes are limited by their 4-digit nature to
 * two digit numbers. Thus, the highest position you can go
 * to is 99, 99 (~9999). You can get around this limit by
 * adding relative cursor movement onto the absolute one,
 * such as ~9940~X+29 to goto 128, 40. This function will
 * generate these codes for you.
 */
string gotoxy_code( int x, int y ) {
   string code, ret;
   int morex;
   int morey;

   if( x > 99 ) { morex = x - 99; x = 99; }
   if( y > 99 ) { morey = y - 99; y = 99; }
   code = "~    ";
   code[1] = y / 10 + '0';
   code[2] = y % 10 + '0';
   code[3] = x / 10 + '0';
   code[4] = x % 10 + '0';
   ret = code;
   while( morex > 0 ) {
      code = "~X+  ";
      code[3] = (morex>99?99:morex) / 10 + '0';
      code[4] = (morex>99?99:morex) % 10 + '0';
      ret += code;
      morex -= 99;
   }
   while( morey > 0 ) {
      code = "~Y+  ";
      code[3] = (morey>99?99:morey) / 10 + '0';
      code[4] = (morey>99?99:morey) % 10 + '0';
      ret += code;
      morey -= 99;
   }

   return ret;
}

/** Moves the cursor to an x,y on the screen. The upper left corner is
 * 1,1 and the x increases to the right, y increases downward. Note
 * that this function, unlike the built in code, will also work for
 * values of x and y over 99.
 */
void gotoxy( int x, int y ) {
   msg_bare( gotoxy_code(x, y) );
}

/// Clears screen and moves cursor to upper-left corner.
void cls() {
   msg_bare("~CLRS~0101");
}

/// Gets the position of the n'th underbar or dash, used by help_frame
protected int _getn( int n, string str ) {
   int i, j;

   j = 0;
   for( i = 0; i < strlen(str) && j < n; i++ )
      if( str[i] == '_' || str[i] == '-') j++;

   return i;
}

/// Gets the most recently used ANSI code from the string.
protected string _get_last_ansi(string str) {
   int i, j;

   j = 0;
   for( i = strlen(str); i && !j;)
   if( str[--i] == 0x1B )
      for( j = i; str[j] != 'm' && j < strlen(str)-1; j++ );
   if( i != j )
      return str[i..j];
   else
      return 0;
}

/** Prints out a text file inside of a help frame. The internal coloring
 * of the help file is still handled within it.
 */
void help_frame(string text) {
   int width;
   string * text_arr, * para_arr;
   string * prefix;
   string line, line2;
   string lastcode, buf, newcode;
   int i, j, k;

   text = implode( explode(text, "\t"), "   " );
   text_arr = list_to_array( text );

   if( sizeof(text) < 2 ) {
      msg("Invalid help file -- missing a section and topic line. Please report a typo on this help file.");
      return;
   }

   width = this_player()->query_term_width() - 1;

   // Top metallic line
   line = "~[150----~[070--~[080----~[070--";
   line2 = "";
   while( cstrlen(line2) < width * 3 ) line2 += line;
   buf = "";
   buf += process_string("."+pad(line2, _getn(width - 2, line2), 0)+".\n", 0);

   // Title line
   line = text_arr[0];
   line2 = line + line;
   j = 0;
   for( i = 0; i < cstrlen(line); i++ ) {
      line2[j] = line[i];
      j++;
      if( line[i] != ' ' ) {
         line2[j] = ' ';
         j++;
      }
   }

   // Strip off any remaining trailing characters...
   line2 = line2[0..j-1];
   i = width - (cstrlen(line2) * 2 + 3);
   if( i < cstrlen(MUD_TITLE) )
      buf += process_string("~[150| ~[020"+pad(text_arr[0], width-4, 2)+"~[070 |\n", 0);
   else
      buf += process_string("~[150| ~[020"+line2+pad("~[100"+MUD_TITLE+"~[020", i, 2)+line2+"~[070|\n", 0);

   // Bottom metallic line
   line = "~[070--~[080----~[070--~[150----";
   line2 = "";
   while( cstrlen(line2) < width * 3 ) line2 += line;
   buf += process_string("~[070|"+pad(line2, _getn(width - 2, line2), 0)+"~[080'\n", 0);

   // Topic line
   buf += process_string("~[070| ~[060-,  - ~[150|  ~CCOM" + text_arr[1]+"\n", 0);
   buf += process_string("~[080| ~[140||--| ~[150|~[040" + tab("-", width-9) +"\n", 0);

   // Set up prefix lines and start printing the text
   prefix = ({
            process_string( "~[080| ~[060-'  - ~[070| ", 0 ),
            process_string( "~[070| ~[060,---- ~[080| ", 0 ),
            process_string( "~[150| ~[140||--  ~[080| ", 0 ),
            process_string( "~[150| ~[060`---- ~[070| ", 0 ),
            process_string( "~[070| ~[060-,    ~[150| ", 0 ),
            process_string( "~[080| ~[140||    ~[150| ", 0 ),
            process_string( "~[080| ~[060-'--' ~[070| ", 0 ),
            process_string( "~[070| ~[060-,--, ~[080| ", 0 ),
            process_string( "~[150| ~[140||--' ~[080| ", 0 ),
            process_string( "~[150| ~[060-'    ~[070| ", 0 ),
            process_string( "~[070'~[080----~[070--~[150-' ", 0),
            process_string( "          ~CHLP", 0 )
            });

   // i loops through the text array, j through the prefixes.
   i = 2;
   j = 0;

   lastcode = process_string("~CHLP", 0);
   while( i < sizeof(text_arr) || j < 11 ) {
      // Get all the text for a paragraph
      line = 0;
      if( i < sizeof(text_arr) ) {
         while( i < sizeof(text_arr) && (!line || (text_arr[i] != "" && text_arr[i][0] != ' ')) ) {
            if( !line ) line = text_arr[i]; else line = trim(line, TRIM_RIGHT) + " " + text_arr[i];
            i++;
         }
         if( !line ) line = " ";
      }
      else line = " ";
      if( i < sizeof(text_arr) && text_arr[i] == "" ) {
         line += "\n";
         i++;
      }
      if( line[0..2] == " * " ) line = " ~CBRT* " + line[3..] + "~CHLP";
      if( line == "" ) line = " ";

//      write("About to handle the line:\n"+line+"\n");
      // Write the paragraph to the screen plus lots of hard-to-undertand processing.
      line = break_string(process_string(line, 0), width - 10, 0);
      para_arr = list_to_array( line );
      for( k = 0; k < sizeof(para_arr); k++ ) {
         buf += prefix[j] + lastcode + para_arr[k] + "\n";
         newcode = _get_last_ansi(para_arr[k]);
         if( newcode ) lastcode = newcode;
         if( j < 11 ) j++;
      }
   }
//   this_player()->receive(buf+process_string("~CDEF", 0));
   write( buf + process_string("~CDEF") );
}

/** Splices \a left with \a right, giving a two-column
 * format. No spacing is placed between the blocks of
 * text. The left block of text should have all lines be
 * of equal width (it will work if they aren't, but you
 * might not like the effect). The right block of text
 * should already be linewrapped to whatever width is
 * necessary.
 *
 * This will handle ANSI color codes, but they should already
 * be converted to the ANSI format: ESC[0;35;45m. You should
 * use process_ansi or process_codes to do this.
 *
 * The optional \a width parameter is to allow you to
 * specify line width. This will probably be necessary
 * if your left-side image uses ANSI codes.
 */
varargs string splice_text( string left, string right, int width ) {
   int i, n;
   string *aleft, *aright;
   string curr_color, left_color, right_color, *left_codes, *right_codes, *final;

   curr_color = left_color = process_codes("~CDEF"); right_color = process_codes("~CDEF");

   aleft = explode( left, "\n" );
   while( aleft[<1] == "" ) aleft = aleft[..<2];
   aright = explode( right, "\n" );
   if( !width ) width = strlen(aleft[<1]);
   while( sizeof(aleft) < sizeof(aright) )
      aleft += ({ tab(" ", width) });
   while( sizeof(aleft) > sizeof(aright) )
      aright += ({ "" });
   n = sizeof(aleft);
   final = ({ });
   for( i = 0 ; i < n; i++ ) {
      string tmp;

      tmp = (i?"\n":"");
      if( curr_color != left_color )
         tmp += curr_color = left_color;
      tmp += aleft[i];

      left_codes = regexplode( aleft[i], "\27\\[[0-9;]*m" );
      if( sizeof(left_codes) > 1 )
         curr_color = left_color = left_codes[<2];

      if( curr_color != right_color )
         tmp += curr_color = right_color;
      tmp += aright[i];

      right_codes = regexplode( aright[i], "\27\\[[0-9;]*m" );
      if( sizeof(right_codes) > 1 )
         curr_color = right_color = right_codes[<2];

      final += ({ tmp });
   }

   return implode( final, "" );
}

/** Applies an alternative coloring scheme for the say/tell/shout etc. commands.
 * This also has the effect of filtering out tilde codes.
 */
string recolor( string orig, string std_color ) {
   string ret, str, a, b, c;

   if( !std_color ) std_color = "~CDEF";

//   write("orig = "+orig+"\n");
   ret = orig;
   ret = implode( explode(ret, "~"), "~TILD" );

//   write("ret = "+ret+"\n");
   while( sscanf(ret, "%sRED(%s)%s", a, b, c) == 3 )
      ret = a + "~[090" +  b + std_color + c;
   while( sscanf(ret, "%sDEFAULT(%s)%s", a, b, c) == 3 )
      ret = a + "~CDEF" +  b + std_color + c;
   while( sscanf(ret, "%sBLUE(%s)%s", a, b, c) == 3 )
      ret = a + "~[120" +  b + std_color + c;
   while( sscanf(ret, "%sGRAY(%s)%s", a, b, c) == 3 )
      ret = a + "~[080" +  b + std_color + c;
   while( sscanf(ret, "%sYELLOW(%s)%s", a, b, c) == 3 )
      ret = a + "~[110" +  b + std_color + c;
   while( sscanf(ret, "%sWHITE(%s)%s", a, b, c) == 3 )
      ret = a + "~[150" +  b + std_color + c;
   while( sscanf(ret, "%sGREEN(%s)%s", a, b, c) == 3 )
      ret = a + "~[100" +  b + std_color + c;

   ret = implode( explode(ret, "$("), "$~LPAR" );
   ret = implode( explode(ret, "?("), "?~LPAR" );
   ret = implode( explode(ret, ":("), ":~LPAR" );
//   write("ret = "+ret+"\n");

   return ret;
}

/** Draws a box on the screen in this style:
 *
 * \c ,-.
 * \c | |
 * \c `-'
 */
void drawbox( int x, int y, int width, int height, string color ) {
   int i;

   gotoxy( x, y );
   msg_bare( color );
   msg_bare("," + tab("-", width-2) + ".");
   for( i = 1; i < height - 1; i++ ) {
      gotoxy( x, y + i );
      msg_bare("|" + tab(" ", width-2) + "|");
   }
   gotoxy( x, y + height - 1 );
   msg_bare("`" + tab("-", width-2) + "'");
}

/** Utility functions that allows you to do a scrolling terminal window
 * inside of a frame (or whatever) on the screen. The terminal window
 * data is an array of strings. Strings processed in this fashion
 * are assumed to have already been translated to their ANSI codes
 * (which is normal given the methods used for string capture).
 */
varargs string * add_text(string *window, string data, int width, int max_height) {
   string *newlines;
   int i;

   if( !pointerp(window) || !stringp(data) ) return window;
   if( width == 0 && this_player() ) width = this_player()->query_term_width();
   if( width <= 0 ) return window;
   if( !max_height ) max_height = sizeof(window);

   // First cut up the text into separate lines.
   newlines = explode( data, "\n" );
   // Wrap lines that exceed the desired width.
   for( i = 0; i < sizeof(newlines); i++ )
      newlines[i] = break_string(newlines[i], width, 0);

   // Re-do the line wrap now.
   newlines = explode( implode( newlines, "\n" ), "\n" );

   // Remove blank lines from off the end.
   while( sizeof(newlines) > 0 && newlines[<1] == "" ) newlines = newlines[..<2];

   // Add the new lines onto the end of the terminal window...
   window += newlines;

   // And clip off lines that have scrolled off the top of the window.
   if( sizeof(window) > max_height )
      window = window[sizeof(window) - max_height..];

   return window;
}

/** \param words The words to be displayed in column form.
 * \param term_width (Optional) The width of the terminal on which the words
 * are to be displayed; defaults to the width of this_player()'s terminal.
 * \param width (Optional) The width of each column, in spaces. If you don't
 * specify a width, it will be autodetected to match the width of the longest
 * entry plus two spaces.
 * \return A string with \a snippets arranged into columns, each
 * column \a width characters wide. Generally, you don't need to specify
 * \a width, because the function will autodetect the width (it is an
 * optional parameter); however, if your strings contain color codes,
 * this will confuse the width detection, and you should specify it.
 *
 * The column format will arrange the text in order vertically. So if
 * your list of words is alphabetized, the alphabetical ordering will
 * have the earliest letters on the left, the later letters on the
 * right, instead of top-to-bottom.
 */
varargs string columns( string *words, int term_width, int width ) {
   int i, j, longest, col, row, cols, rows;
   string buf;

   if( !pointerp(words) || sizeof(words) == 0 ) return "";

   if( !width ) {
      width = 0;
      // Find longest name for column width
      for( i = 0; i < sizeof(words); i++ )
         if( strlen(words[i]) > width ) width = strlen(words[i]);
      width += 2;
   }

   if( !term_width )
      if( this_player() )
         term_width = this_player()->query_term_width();
      else
         term_width = 79;

   // Determine # of columns & rows. The - 1 is so that
   // I don't get accidental wraparound.
   cols = (term_width - 1) / width;
   if( cols <= 0 ) cols = 1;
   rows = 1 + (sizeof(words) - 1) / cols;

   buf = "";
   col = 0;
   row = 0;
   for( i = 0; i < rows * cols; i++ ) {

      j = (i % cols) * rows + (i / cols);

      if( j < sizeof(words) )
         buf += pad( words[j], width, 0 );

      col++;
      if( col >= cols  ) {
         col = 0;
         row++;
         buf += "\n";
      }
   }

   if( (rows * cols) % cols != 0 ) buf += "\n";
   return buf;
}

/** Draws the text stored in the array "window" to the screen.
 * The left side of the first line of text is at x, y, with
 * other text arranged vertically below it. Note that this
 * function uses the EFUN tell_object, which will bypass any text
 * capturing you have. As usual this doesn't do quite what
 * you want if you don't have ANSI enabled. Passing in the
 * user is also important, since this_player() will generally
 * not correspond to the person viewing the terminal. This does use ~CEOL so
 * don't try putting a border on the right side (or be willing
 * to redraw it each time).
 */
void draw_terminal(string *window, int x, int y, object user) {
   int ly;
   string buf;

   buf = process_string("~SAVE~CDEF", 0);
   for( ly = 0; ly < sizeof(window); ly++ ) {
      buf += process_string(gotoxy_code( x, y + ly ), 0);
      buf += window[ly] + process_string("~CEOL", 0);
   }
   buf += process_string("~RECL", 0);

   efun::tell_object(user, buf);
}

/// Internal, used by line input boxes
nosave int firstchar, xleft, boxwidth, line, curs;
/// Internal, used by line input boxes
nosave string line_edit_keybuf, curr_buf, edit_return_func;

/// A debug function, used for checking on the state of an input box.
void debug_list() {
   msg("fc: "+firstchar+"\nxleft: "+xleft+"\nboxwidth: "+boxwidth+"\nline: "+line+"\ncurs: "+curs+
      "\nline_edit_keybuf: "+cleanup_string(line_edit_keybuf)+"\ncurr_buf: "+curr_buf+"\nedit_return_func: "+edit_return_func);
}

/// Displays the contents of a line input box.
static void display_line_input() {
   string disp;

   gotoxy( xleft, line );

   disp = pad( curr_buf[firstchar..], boxwidth, JUST_LEFT );
   write( disp );
   gotoxy( xleft + curs - firstchar, line );
}

/// An internal function used by line_input to handle the interface.
void line_input_interface( string c ) {
   int code;

   if( !c ) {
      display_line_input();
      return;
   }

   if( c[0] < 32 || c[0] > 126 || line_edit_keybuf != "" ) {
      line_edit_keybuf += c;
      code = this_player()->translate_code( line_edit_keybuf );
      if( code == KEY_NONE ) return;
      line_edit_keybuf = "";
      switch( code ) {
         case KEY_LEFT:
            if( curs > 0 ) {
               curs--;
               if( curs < firstchar ) firstchar -= boxwidth / 2;
               display_line_input();
            }
            return;
         case KEY_RIGHT:
            if( curs < strlen(curr_buf) ) {
               curs++;
               if( curs >= firstchar + boxwidth ) firstchar += boxwidth / 2;
               display_line_input();
            }
            return;
         case KEY_END:
            curs = strlen(curr_buf);
            while( curs >= firstchar + boxwidth ) firstchar += boxwidth / 2;
            display_line_input();
            return;
         case KEY_HOME:
            curs = 0;
            firstchar = 0;
            display_line_input();
            return;
         case KEY_BACKSPACE:
            if( curs > 0 ) {
               curr_buf = curr_buf[..curs-2] + curr_buf[curs..];
               curs--;
               display_line_input();
            }
            return;
         case KEY_DELETE:
            if( curs < strlen(curr_buf) ) {
               curr_buf = curr_buf[..curs-1] + curr_buf[curs+1..];
               display_line_input();
            }
            return;
         case KEY_CR:
            drop_interface();
            call_other( this_object(), edit_return_func, curr_buf );
            return;
      }
   }
   else {
      if( curs == strlen(curr_buf) )
         curr_buf += c;
      else
         curr_buf = curr_buf[..curs-1] + c + curr_buf[curs..];
      curs++;
      if( curs >= firstchar + boxwidth ) firstchar += boxwidth / 2;
      display_line_input();
   }
}

// Will get working, then will document afterwards...
// Note: MUST #include <textdraw.h> for line input to work properly, and
// you may only have one line input per object. This function may
// not be called remotely.
/** This function is designed to help you build full-blown text editors.
 * This function has several 'state' variables which are stored inside of the textdraw object,
 * meaning you must inherit the textdraw object (you can't use this function with call_other).
 * Also, you cannot stack it -- one line_input must finish before the next one is started.
 */
static varargs void line_input( int _xleft, int _width, int _line, int _curs, string _edit_return_func, string initial_text ) {
   if( !stringp(initial_text) )
      curr_buf = "";
   else
      curr_buf = initial_text;

   if( !stringp(_edit_return_func) )
      edit_return_func = "line_input_return";
   else
      edit_return_func = _edit_return_func;

   xleft = _xleft;
   boxwidth = _width;
   line = _line;
   curs = _curs;
   line_edit_keybuf = "";
   if( xleft < 1 ) xleft = 1;
   if( line < 1 ) line = 1;
   if( boxwidth < 1 ) boxwidth = 20;
   if( curs < 0 ) curs = 0;
   firstchar = 0;
   while( curs >= firstchar + boxwidth ) firstchar += boxwidth / 2;
   add_interface("@line_input_interface", 0);
   display_line_input();
}
