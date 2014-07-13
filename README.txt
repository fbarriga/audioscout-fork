Audio Scout content indexing software
version 1.0.1
creator: D. Grant Starkweather & Evan Klinger
contact: dstarkweather@phash.org, eklinger@phash.org


BACKGROUND
-----------------------------------------------------------------------
  Audio Scout is audio content indexing software.  For more of a
  detailed explanation, please read audioscoutm.pdf. 


INSTALLATION:
-----------------------------------------------------------------------

   Use the cmake build system. From the top level directory:

     cd ./build
     cmake ../.
     make all
     sudo make install

   This should build the libraries, the servers, the client application (both
   console and qt gui) and the driver and test programs.

   NOTE: In order for cmake to recognize Qt4 installation, the bin directory in
   which qmake program is installed might have to be on the system PATH.


   Files:

   /libs - source code for the AudioData and pHashAudio libraries.

   /clients - source code for a console based client app (auscout-client.c)
              and a qt gui app.

   /servers - source code for the server programs (auscoutd, tblservd, metadatadb)

   /tests   - source code for test programs and driver programs to test the server
              performance.

   /docs    - audioscoutm.pdf manual

INSTRUCTIONS
-----------------------------------------------------------------------
	1. Create sqlite database for the audio file metadata storage.
	
		sqlite3 audio.db

           In the sqlite environ, 		

		.read audiodb.sql

	2. Start the metadatadb server against this database file.

	   ./metadatadb -i /path/to/database/file/audio.b

	   Be sure to use an absolute path. Check the -h option 
           for other settings you might want to make.
		
        3. (Optional) Build the index files on local machines using the
           'audioindex' utility program. This is optional.  However,
           if you already have alot of files you want to index, this
           provides a faster way, since you can start building on multiple
           machines.  Just type:

		      ./audioindex

           to get a list of the program options.   

	4. Start the auscoutd server with the address to find the metadatadb
           server.

	   ./auscoutd -d tcp://<address>:<metadatadb_port>

		      e.g. ./auscoutd -d tcp://localhost:4000


	5. Start the table servers.  This might only be one server but could be any
           number of table servers (up to 255).  
	    
	    ./tblservd -s <address of auscoutd> -p <auscoutd port> -i /path/to/index/file

	            e.g. ./tblservd -s 192.168.1.100 -p 4005 -i /home/usr/audiodb

	   Be sure to use full path to the index file.  If you did not create an index file
           in step 3, an empty index will be created with the name you provide.  Use the -h 
           option to find out about the other options.

	6. Use client program to to query the index with unknown files or add new
           unindexed files to the system.  

	   There is a simple console based program in auscout-client.c that demonstrates how
           to use the api programatically.

	   Use the program names 'auscout' to test out the system.  It is GUI front end based
           on Qt.  It should be straightforward to use:

	      1. You can set the connection to the auscoutd server through the connect button or
                 the menu item.  If it is a wrong connection, a message box will inform you.

	      2. You can sample from a the mic through the sample interface.  A list box allows
                 you to choose the device.  You can also set the amplitude gain adjustment that
                 is applied to the signal and the sample interval in seconds through the menu
                 items.

              3. Simply click the sample button to begin sampling from the mic.  A meter will
                 show the measured signal as it is recieved.  A look up is attempted when the
                 sampling is finished.  View the process in the display.

              4. The system works in two modes: query and submit.  You can submit new files to 
                 be indexed by selecting the 'submit' radio button.  Or you can choose to query
                 for files by selecting the 'query' radio button.  Browse and select which files
                 you want to send and click the 'hash & send' button when you are ready.
         
   

REQUISITE SOFTWARE
-----------------------------------------------------------------------
zeromq version 2.0.2 www.zeromq.org
sqlite3 version 3.6.23 www.sqlite.org
sndfile version 1.0.21 www.mega-nerd.com/libsndfile

libOGG 1.2.0   (required by sndfile for ogg and flac support)
               (NOTE: be sure to install ogg before flac!)
libFLAC 1.2.1   http://xiph.org/downloads
libvorbis 1.3.0

samplerate version 0.1.7 www.mega-nerd.com/SRC (NOTE: install sndfile before samplerate)
mpg123 version 1.12.1 www.mpg123.de/api (for mp3 support)

Qt 4.7.1 (required for the GUI client application)