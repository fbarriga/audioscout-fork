using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Runtime.InteropServices;
using ZMQ;
using PHash;

namespace auscout
{
    class auscout
    {

        static void Main(string[] args)
        {
            if (args.Length < 4)
            {
                Console.WriteLine("not enough args");
                Console.WriteLine("usage: prog command dirPath serverAddress nbseconds");
                Console.WriteLine("command - 1 for query, 2 for submit files");
                Console.WriteLine("dirPath - path of directory containing audio files");
                Console.WriteLine("server  - address of audioscout server, e.g. tcp://127.0.0.1:4005");
                Console.WriteLine("nbseconds - first number of seconds to sample from each file (0 if for the whole file)");
                return;
            }

            //parse arguments 
            byte[] cmd = { Byte.Parse(args[0]) };
            string dirPath = args[1];
            string addr = args[2];
            Single nbsecs = Single.Parse(args[3]);
            int err = 0, sr = 6000;

            //separation string for metadata parts 
            // composer RS title RS performer RS date RS album RS genre RS year RS duration RS part
            // RS == record separator character (value 32)
            const string sepStr = "\u0020\u001e\u0020";

            AudioData.AudioMetaData mdatast = new AudioData.AudioMetaData();
            PHashAudio.AudioHashStInfo hashst = new PHashAudio.AudioHashStInfo();

            // set up zeromq messaging 
            Context context;
            Socket skt;
            try
            {
                context = new Context(1);
                skt = context.Socket(SocketType.REQ);
                skt.Connect(addr);
            }
            catch (ZMQ.Exception ex)
            {
                Console.WriteLine("Unable to connect socket: " + ex.Message);
                Console.WriteLine(ex.StackTrace);
                return;
            }

            //enumerate through files in dir
            var files = Directory.EnumerateFiles(dirPath, "*.mp3", SearchOption.AllDirectories);
            foreach (var file in files)
            {
                Console.WriteLine("file: " + file);

                // read signal and metadata 
                Single[] buf = AudioData.readaudio(file, sr, nbsecs, ref mdatast, ref err);
                if (buf == null)
                {
                    Console.WriteLine("Unable to read file - error " + err);
                    continue;
                }

                Console.WriteLine("signal: " + buf.Length + " samples");

                // calc hash for signal
                Int32[] hasharray = PHashAudio.audiohash(buf, sr, ref hashst);
                if (hasharray == null)
                {
                    Console.WriteLine("Unable to calculate hash array");
                }

                Console.WriteLine("hash: " + hasharray.Length + " frames");

                byte[] nb = BitConverter.GetBytes(hasharray.Length);
                byte[] hashbytes = new byte[sizeof(Int32) * hasharray.Length];
                Buffer.BlockCopy(hasharray, 0, hashbytes, 0, sizeof(Int32) * hasharray.Length);


                if (cmd[0] == 1)   //query
                {
                    Console.WriteLine("Querying signal ...");
                    try
                    {
                        // send message
                        skt.SendMore(cmd);
                        skt.SendMore(nb);
                        skt.Send(hashbytes);

                        Thread.Sleep(1000);

                        //wait for reply
                        byte[] resp = skt.Recv(1000);
                        string respStr = Encoding.UTF8.GetString(resp);
                        Console.WriteLine("Result: " + respStr);
                    }
                    catch (ZMQ.Exception ex)
                    {
                        Console.WriteLine("ZMQ Error: " + ex.Message);
                        Console.WriteLine();
                        return;
                    }

                    Console.WriteLine();
                    Console.WriteLine("******Hit Enter Key********");
                    Console.ReadLine();
                }
                else if (cmd[0] == 2) //submit
                {  
                    // construct inline metadata string from metadata structure 
                    StringBuilder metadataStr = new StringBuilder(512);
                    metadataStr.Append(AudioData.getString(mdatast.composer)); //composer
                    metadataStr.Append(sepStr);
                    metadataStr.Append(AudioData.getString(mdatast.title1));   //title
                    metadataStr.Append(AudioData.getString(mdatast.title2));
                    metadataStr.Append(AudioData.getString(mdatast.title3));
                    metadataStr.Append(sepStr);
                    metadataStr.Append(AudioData.getString(mdatast.tpe1));     //performer
                    metadataStr.Append(AudioData.getString(mdatast.tpe2));
                    metadataStr.Append(AudioData.getString(mdatast.tpe3));
                    metadataStr.Append(AudioData.getString(mdatast.tpe4));
                    metadataStr.Append(sepStr);
                    metadataStr.Append(AudioData.getString(mdatast.date));     //date
                    metadataStr.Append(sepStr);
                    metadataStr.Append(AudioData.getString(mdatast.album));    //album
                    metadataStr.Append(sepStr);
                    metadataStr.Append(AudioData.getString(mdatast.genre));    //genre
                    metadataStr.Append(sepStr);
                    metadataStr.Append(mdatast.year);                          //year
                    metadataStr.Append(sepStr);
                    metadataStr.Append(mdatast.duration);                      //duration
                    metadataStr.Append(sepStr);
                    metadataStr.Append(mdatast.partofset);                     //part
                    

                    string mdataStr = metadataStr.ToString();

                    // need to send a byte[] with a null char in the last position
                    // Is there a better way to convert the string ????
                    byte[] mdataBytes = Encoding.UTF8.GetBytes(mdataStr);
                    byte[] mdataBytes1 = new byte[mdataBytes.Length + 1];
                    Buffer.BlockCopy(mdataBytes, 0, mdataBytes1, 0, mdataBytes.Length);
                    mdataBytes1[mdataBytes.Length] = 0x00;

                    Console.WriteLine("submitting: " + mdataStr);
                    Console.WriteLine();


                    try
                    {
                        // send message
                        skt.SendMore(cmd);
                        skt.SendMore(nb);
                        skt.SendMore(hashbytes);
                        skt.Send(mdataBytes1);


                        Thread.Sleep(1000);

                        //wait for reply
                        byte[] resp = skt.Recv(5000);

                        if (resp != null)
                        {
                            int uid = BitConverter.ToInt32(resp, 0);
                            Console.WriteLine("successfully submitted - uid is " + uid);
                        }
                        else
                        {
                            Console.WriteLine("no response from server");
                        }
                    }
                    catch (ZMQ.Exception ex)
                    {
                        Console.WriteLine("ZMQ error: " + ex.Message);
                    }
                }
                else
                {
                    Console.WriteLine("unknown cmd: " + cmd[0]);
                }

                Console.WriteLine();
            }

            AudioData.free_mdata(ref mdatast);
            PHashAudio.ph_hashst_free(ref hashst);

        }
    }
}
   
