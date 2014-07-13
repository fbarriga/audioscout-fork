package org.phash;

public class TestAudio {

    public static void main(String[] args){
	if (args.length < 1){
	    System.out.println("not enough args");
	    System.out.println("Usage: prog <file>");
	    return;
	}

	String file = args[0];
	int sr = 6000;
	int P = 8;
	float nbsecs = 0.0f;
	System.out.println("file: " + file);

	AudioMetaData mdataObj = new AudioMetaData();

	float[] samples = AudioHash.readAudio(file, sr, nbsecs, mdataObj);

	System.out.println("nb samples: " + samples.length);
	System.out.println("sample rate/sec: " + sr);

	System.out.println("composer: " + mdataObj.composer);
	System.out.println("title: " + mdataObj.title1 + " " + mdataObj.title2 + " " + mdataObj.title3);
	System.out.println("artist: " + mdataObj.tpe1 + " - " + mdataObj.tpe2 + " - " + mdataObj.tpe3 + " - " + mdataObj.tpe4);
	System.out.println("date: " + mdataObj.date);
	System.out.println("genre: " + mdataObj.genre);
	System.out.println("year: " + mdataObj.year);
	System.out.println("duration: " + mdataObj.duration);
	System.out.println("part: " + mdataObj.partofset);
	System.out.println("");

	AudioHashObject hashObj = AudioHash.audioHash(samples, P, sr);

	System.out.println("Audio Hash, nbframes " + hashObj.hash.length + " , P = " + P);
	for (int i=0;i<hashObj.hash.length;i+=1000){
	    System.out.print("hash[" + i + "] = " + hashObj.hash[i] + " ");
	    for (int j=0;j<P;j++){
		System.out.print(" " + hashObj.toggles[i][j] + " ");
	    }
	    System.out.println();
	}
	System.out.println("Done.");

	return;
    }

}
