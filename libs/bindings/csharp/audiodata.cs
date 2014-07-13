/*      Audio Scout - audio content indexing software      
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

namespace PHash
{

  public class AudioData
  {
    [StructLayout(LayoutKind.Sequential)]
    public struct AudioMetaData
    {
      public IntPtr composer;
      public IntPtr title1;
      public IntPtr title2;
      public IntPtr title3;

      public IntPtr tpe1;
      public IntPtr tpe2;
      public IntPtr tpe3;
      public IntPtr tpe4;

      public IntPtr date;

      public Int32 year;

      public IntPtr album;
      public IntPtr genre;

      public Int32 duration;
      public Int32 partofset;
    }

    public static string getString(IntPtr ptr)
    {
      return Marshal.PtrToStringAnsi(ptr);
    }
  
    /// <summary>
    ///   Read in an audio file and return a buffer of samples at given samplerate, sr
    /// </summary>
    /// <param name="strign">name of file</param>
    /// <param name="Int32">sr samplerate of samples, e.g. 6000 samples per sec</param>
    /// <param name="float*">buf pre-allocated buffer of floats in which to put the signal, can be null,
    ///                      in which case a buffer is allocated by the function.</param>
    /// <param name="UInt32">buflen, reference to unsigned int to return the length of the buffer.</param>
    /// <param name="float">nbsecs, number of seconds to read from the file, 0 for complete file.</param>
    /// <param name="AudioMetaData">mdata, reference to struct AudioMetaData to return metadata in file.</param>
    /// <param name="int">error, ref to int to indicate error code in case of error.</param>
    /// <returns>float[] array of samples  </returns>

    public static float[] readaudio(string filename, 
                                    Int32 sr, 
                                    float nbsecs,
                                    ref AudioMetaData mdata, 
                                    ref Int32 error)
    {
      UInt32 buflen = 0;
      IntPtr buf = readaudio(filename, sr, null, ref buflen, nbsecs, ref mdata, ref error); 

      float[] buffer = new float[(Int32)buflen];
      Marshal.Copy(buf, buffer, 0, (Int32)buflen);

      audiodata_free(buf);
      return buffer;
    }


    [DllImport("libAudioData.dll", CharSet=CharSet.Ansi, CallingConvention=CallingConvention.Cdecl)]
    private extern static IntPtr readaudio(String filename, 
                                           Int32 sr, 
					   [ In, Out, MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.R4)] 
                                           float[] buf, 
                                           ref UInt32 buflen,
                                           float nbsecs,
                                           ref AudioMetaData mdata,
                                           ref Int32 error);



    /// <summary>
    ///   free mdata struct after call to readaudio
    /// </summary>
    /// <param name="ref AudioMetaData">mdata reference to AudioMetaData struct</param>
    /// <returns> void </returns>
    [DllImport("libAudioData.dll", CallingConvention=CallingConvention.Cdecl)]
    public extern static void free_mdata(ref AudioMetaData mdata);

    [DllImport("libAudioData.dll", CallingConvention=CallingConvention.Cdecl)]
    private extern static void audiodata_free(IntPtr ptr);

    /// <summary>
    ///   private ctor to keep from being instantiated, all methods are static
    /// </summary>
    private AudioData(){ }

  } 
}
