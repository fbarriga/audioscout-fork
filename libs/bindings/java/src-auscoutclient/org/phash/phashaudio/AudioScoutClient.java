package org.phash.phashaudio;
import java.io.File;
import java.io.IOException;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.phash.*;
import org.zeromq.ZMQ;
import org.zeromq.ZMQ.Socket;
import org.zeromq.ZMQ.Context;

public class AudioScoutClient {

    public static byte[] metaDataInlineBytes(AudioMetaData mdataObj){
	//composer(str) | title(str) | perf(str) | date(str) | album(str) | genre(str) |
	//year (int)    | dur (int)  | part(int) 
	StringBuilder metadataStr = new StringBuilder(512);
	if (mdataObj.composer != null) metadataStr.append(mdataObj.composer);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	if (mdataObj.title1 != null) metadataStr.append(mdataObj.title1);
	metadataStr.append(" ");
	if (mdataObj.title2 != null) metadataStr.append(mdataObj.title2);
	metadataStr.append(" ");
	if (mdataObj.title3 != null) metadataStr.append(mdataObj.title3);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	if (mdataObj.tpe1 != null) metadataStr.append(mdataObj.tpe1);
	metadataStr.append(" ");
	if (mdataObj.tpe2 != null) metadataStr.append(mdataObj.tpe2);
	metadataStr.append(" ");
	if (mdataObj.tpe3 != null) metadataStr.append(mdataObj.tpe3);
	metadataStr.append(" ");
	if (mdataObj.tpe4 != null) metadataStr.append(mdataObj.tpe4);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	if (mdataObj.date !=  null) metadataStr.append(mdataObj.date);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	if (mdataObj.album != null) metadataStr.append(mdataObj.album);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	if (mdataObj.genre != null) metadataStr.append(mdataObj.genre);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	metadataStr.append(mdataObj.year);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	metadataStr.append(mdataObj.duration);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(30);
	metadataStr.append(" ");
	metadataStr.append(mdataObj.partofset);
	metadataStr.append(" ");
	metadataStr.appendCodePoint(0);

	byte[] mdataBytes = metadataStr.toString().getBytes();

	return mdataBytes;
    }

    public static void main(String[] args){
	if (args.length < 5){
	    System.out.println("not enough args");
	    System.out.println("Usage: prog cmd dir addr nbtoggles nbsecs");
	    System.out.println("cmd - 1 for query, 2 for submit");
	    System.out.println("dir - directory from which to take audio files");
	    System.out.println("addr - address of auscout server, e.g. tcp://ddd.ddd.d.ddd:p");
	    System.out.println("nbtoggles - number of bits to toggle per hash in order to increase candidates for lookup.");
	    System.out.println("nbsecs    - number of seconds of samples to take from each file");
	    return;
	}
	final int sr = 6000;
	final int command = Integer.parseInt(args[0]);
	final byte[] cmdByteArray = { (byte)command };
	final String dirName = args[1];
	final String addr = args[2];
	final int nbtoggles = Integer.parseInt(args[3]);
	final float nbsecs = Float.parseFloat(args[4]);
	final byte[] pbyteArray = { (byte)nbtoggles };

	if (command != 1 && command != 2){
	    System.out.println("invalid cmd arg");
	    return;
	}

	System.out.println("command: " + command);
	System.out.println("process files in: " + dirName);
	System.out.println("server addr: " + addr);
	System.out.println("nbtoggles: " + nbtoggles);
	System.out.println("nbsecs from each file: " + nbsecs);

	File fileDirectory = new File(dirName);
	File[] files = fileDirectory.listFiles();

	AudioMetaData mdataObj = new AudioMetaData();

	Context ctx = ZMQ.context(1);

	Socket skt = ctx.socket(ZMQ.REQ);
	skt.connect(addr);

	for (File file : files){
	    System.out.println("file: " + file.getName());
	    System.out.println("");

	    float[] buf = AudioHash.readAudio(file.getAbsolutePath(), sr, nbsecs, mdataObj);

	    if (mdataObj.composer != null) System.out.println("composer: " + mdataObj.composer);
	    if (mdataObj.title1 != null)   System.out.println("title: " + mdataObj.title1);
	    if (mdataObj.title2 != null)   System.out.println("title: " + mdataObj.title2);
	    if (mdataObj.title3 != null)   System.out.println("title: " + mdataObj.title3);
	    if (mdataObj.tpe1 != null)     System.out.println("performer: " + mdataObj.tpe1);
	    if (mdataObj.tpe2 != null)     System.out.println("performer: " + mdataObj.tpe2);
	    if (mdataObj.tpe3 != null)     System.out.println("performer: " + mdataObj.tpe3);
	    if (mdataObj.tpe4 != null)     System.out.println("performer: " + mdataObj.tpe4);
	    if (mdataObj.date != null)     System.out.println("date: " + mdataObj.date);
	    if (mdataObj.album != null)    System.out.println("album: " + mdataObj.album);
	    if (mdataObj.genre != null)    System.out.println("genre: " + mdataObj.genre);
	    if (mdataObj.year != 0)        System.out.println("year: " + mdataObj.year);
	    if (mdataObj.duration != 0)    System.out.println("duration: " + mdataObj.duration);
	    if (mdataObj.partofset != 0)   System.out.println("part: " + mdataObj.partofset);

	    AudioHashObject hashObj = AudioHash.audioHash(buf, nbtoggles, sr);

	    int nbframes = hashObj.hash.length;
	    byte[] nbframesarray = new byte[4];
	   
	    nbframesarray[0] = (byte) nbframes;
	    nbframesarray[1] = (byte)(nbframes >> 8);
	    nbframesarray[2] = (byte)(nbframes >> 16);
	    nbframesarray[3] = (byte)(nbframes >> 24);

	    byte[] hasharray = new byte[4*nbframes];
	    for (int i=0;i < nbframes;i ++){
		int hashval = hashObj.hash[i];
		hasharray[4*i  ] =  (byte) hashval;
		hasharray[4*i+1] =  (byte)(hashval >> 8);
		hasharray[4*i+2] =  (byte)(hashval >> 16);
		hasharray[4*i+3] =  (byte)(hashval >> 24);
	    }
	    
	    byte[] reply = null;
	    byte[] inlineBytes = null;
	    switch(command){
	    case 1: // query command
		skt.send(cmdByteArray, ZMQ.SNDMORE);
		skt.send(nbframesarray,ZMQ.SNDMORE);
		
		if (nbtoggles > 0){
		    skt.send(hasharray,  ZMQ.SNDMORE);
		    skt.send(pbyteArray, ZMQ.SNDMORE);
		    for (int i=0;i<hashObj.toggles.length - 1;i++){
			skt.send(hashObj.toggles[i], ZMQ.SNDMORE);
		    }
		    skt.send(hashObj.toggles[hashObj.toggles.length-1], 0);
		} else {
		    skt.send(hasharray, 0);
		}

		System.out.println("wait for reply ...");
		reply = skt.recv(0);
		String replyMsg = new String(reply);
		System.out.println("result: " + replyMsg);
		System.out.println("************************");
		try {
		    System.in.read();
		} catch (IOException ex){
		    ex.printStackTrace();
		}
		break;
	    case 2: // submit command
		inlineBytes = metaDataInlineBytes(mdataObj);

		// send submission
		skt.send(cmdByteArray, ZMQ.SNDMORE);
		skt.send(nbframesarray, ZMQ.SNDMORE);
		skt.send(hasharray, ZMQ.SNDMORE);
		skt.send(inlineBytes, 0);

		//wait for reply 
		System.out.println("wait for reply ...");
		reply = skt.recv(0);
		int uid = ByteBuffer.wrap(reply).order(ByteOrder.LITTLE_ENDIAN).getInt();
		System.out.println("reply: ID# " + uid);
		System.out.println("**********************");
		System.out.println("");
		break;
	    }

	}

	System.out.println("Done.");
	skt.close();
	ctx.term();
	return;
    }

}
