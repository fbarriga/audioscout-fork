/*      
	Audio Scout - audio content indexing software      
        Copyright (C) 2010  D. Grant Starkweather & Evan Klinger            
      	Audio Scout is free software: you can redistribute it and/or modify      
 	it under the terms of the GNU General Public License as published by      
	the Free Software Foundation, either version 3 of the License, or      
	(at your option) any later version.        

	This program is distributed in the hope that it will be useful,      
	but WITHOUT ANY WARRANTY; without even the implied warranty of      
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      
	GNU General Public License for more details.        
	You should have received a copy of the GNU General Public License      
	along with this program.  If not, see <http://www.gnu.org/licenses/>.        

	D. Grant Starkweather - dstarkweather@phash.org      
	Evan Klinger          - eklinger@phash.org  

*/

using System;
using System.Runtime.InteropServices;
using PHash;

class TestPHashAudioBindings
{
  static int Main(string[] args)
  {
    if (args.Length < 1)
      {
	System.Console.WriteLine("not enough args");
	System.Console.WriteLine("usage:");
	System.Console.WriteLine("TestNI.exe <audiofile>");
	return 0;
      }

    string file = args[0];
    System.Console.WriteLine("file: " + file);

    // create struct to hold AudioMetaData struct
    AudioData.AudioMetaData mdata = new AudioData.AudioMetaData();

    // read audio 
    Int32 err = 0;
    Int32 sr = 6000;
    Int32 P = 9;
    float[] buf = AudioData.readaudio(file, 6000, sr, ref mdata, ref err);
    if (buf == null)
      {
	System.Console.WriteLine("error reading audio: " + err);
	return -1;
      }

    // print metadata information for file.
    System.Console.WriteLine("buffer contains " + buf.Length + " samples");
    System.Console.WriteLine("composer: " + AudioData.getString(mdata.composer));
    System.Console.WriteLine("title2: " + AudioData.getString(mdata.title2));
    System.Console.WriteLine("pe1: " + AudioData.getString(mdata.tpe1));
    System.Console.WriteLine("date: " + AudioData.getString(mdata.date));
    System.Console.WriteLine("year: " + mdata.year);
    System.Console.WriteLine("album: " + AudioData.getString(mdata.album));
    System.Console.WriteLine("genre: " + AudioData.getString(mdata.genre));
    System.Console.WriteLine("duration: " + mdata.duration);
    System.Console.WriteLine("partofset: " + mdata.partofset);

    // create AudioHashStInfo struct
    // This holds information commonly used information that can be used
    // across hash calculations, to keep from having to realloc and recalc.
    IntPtr hashst = IntPtr.Zero;

    // calculate hash for signal buffer 
    byte[][] toggles = null;
    Int32[] hasharray = PHashAudio.audiohash(buf, sr, P, ref toggles, ref hashst);
    if (hasharray == null)
      {
	System.Console.WriteLine("problem calculating hash.");
	return -1;
      }

    for (int i=0;i<hasharray.Length;i+=1000){
    	System.Console.Write(hasharray[i] + " ");
	for (int j=0;j<P;j++){
	    System.Console.Write(" " + toggles[i][j] + " ");
	}
	System.Console.WriteLine("");
    }
    System.Console.WriteLine("buffer hashed to " + hasharray.Length + " frames");

    // cleanup members of struct AudioMetaData
    AudioData.free_mdata(ref mdata);

    // cleanup members of struct AudioHashStInfo
    // Invoke when finished hashing a group of files.
    PHashAudio.ph_hashst_free(hashst);

  
    return 0;   
  }
}