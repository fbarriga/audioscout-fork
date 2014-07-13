#include <stdint.h>
#include <jni.h>
#include "org_phash_AudioHash.h"

extern "C" {
#include "../../phash_audio.h"
#include "../../audiodata.h"
}

JNIEXPORT jfloatArray JNICALL Java_org_phash_AudioHash_readAudio
                     (JNIEnv *env, jclass cl, jstring name, jint sr, jfloat nbsecs, jobject mdataObj){

    jboolean iscopy;
    const char *filename = env->GetStringUTFChars(name, &iscopy);

    int error;
    unsigned int buflen = 0;
    AudioMetaData mdata;
    init_mdata(&mdata);
    float *buf = readaudio(filename, sr, NULL, &buflen, nbsecs, &mdata, &error);
    if (buf == NULL){
	free_mdata(&mdata);
	env->ReleaseStringUTFChars(name, filename);
	return NULL;
    }
    jfloatArray buf2 = env->NewFloatArray((jint)buflen);
    env->SetFloatArrayRegion(buf2, 0, buflen, (jfloat*)buf);

    // get fieldIds
    jclass mdataclass      = env->GetObjectClass(mdataObj);

    jfieldID composerfield  = env->GetFieldID(mdataclass, "composer" , "Ljava/lang/String;" );
    jfieldID title1field    = env->GetFieldID(mdataclass, "title1"   , "Ljava/lang/String;" );
    jfieldID title2field    = env->GetFieldID(mdataclass, "title2"   , "Ljava/lang/String;" );
    jfieldID title3field    = env->GetFieldID(mdataclass, "title3"   , "Ljava/lang/String;" );
    jfieldID tpe1field      = env->GetFieldID(mdataclass, "tpe1"     , "Ljava/lang/String;" );
    jfieldID tpe2field      = env->GetFieldID(mdataclass, "tpe2"     , "Ljava/lang/String;" );
    jfieldID tpe3field      = env->GetFieldID(mdataclass, "tpe3"     , "Ljava/lang/String;" );
    jfieldID tpe4field      = env->GetFieldID(mdataclass, "tpe4"     , "Ljava/lang/String;" );
    jfieldID datefield      = env->GetFieldID(mdataclass, "date"     , "Ljava/lang/String;" );
    jfieldID albumfield     = env->GetFieldID(mdataclass, "album"    , "Ljava/lang/String;" );
    jfieldID genrefield     = env->GetFieldID(mdataclass, "genre"    , "Ljava/lang/String;" );
    jfieldID yearfield      = env->GetFieldID(mdataclass, "year"     , "I");
    jfieldID durationfield  = env->GetFieldID(mdataclass, "duration" , "I");
    jfieldID partofsetfield = env->GetFieldID(mdataclass, "partofset", "I");
    
    // set AudioMetaData fields 
    if (composerfield != 0) env->SetObjectField(mdataObj, composerfield,  env->NewStringUTF(mdata.composer));
    if (title1field != 0)   env->SetObjectField(mdataObj, title1field,    env->NewStringUTF(mdata.title1));
    if (title2field != 0)   env->SetObjectField(mdataObj, title2field,    env->NewStringUTF(mdata.title2));
    if (title3field != 0)   env->SetObjectField(mdataObj, title3field,    env->NewStringUTF(mdata.title3));
    if (tpe1field   != 0)   env->SetObjectField(mdataObj, tpe1field,      env->NewStringUTF(mdata.tpe1));
    if (tpe2field   != 0)   env->SetObjectField(mdataObj, tpe2field,      env->NewStringUTF(mdata.tpe2));
    if (tpe3field   != 0)   env->SetObjectField(mdataObj, tpe3field,      env->NewStringUTF(mdata.tpe3));
    if (tpe4field   != 0)   env->SetObjectField(mdataObj, tpe4field,      env->NewStringUTF(mdata.tpe4));
    if (datefield   != 0)   env->SetObjectField(mdataObj, datefield,      env->NewStringUTF(mdata.date));
    if (albumfield  != 0)   env->SetObjectField(mdataObj, albumfield,     env->NewStringUTF(mdata.album));
    if (genrefield  != 0)   env->SetObjectField(mdataObj, genrefield,     env->NewStringUTF(mdata.genre));
    if (yearfield   != 0)   env->SetIntField(mdataObj   , yearfield,      mdata.year);
    if (durationfield != 0) env->SetIntField(mdataObj   , durationfield,  mdata.duration);
    if (partofsetfield != 0)env->SetIntField(mdataObj   , partofsetfield, mdata.partofset);

    audiodata_free(buf);
    free_mdata(&mdata);
    env->ReleaseStringUTFChars(name, filename);

    return buf2;
}


JNIEXPORT jobject JNICALL Java_org_phash_AudioHash_audioHash(JNIEnv *env, jclass, jfloatArray buf, jint P, jint sr) {

    
    jboolean iscopy;
    uint32_t *hash = NULL;
    AudioHashStInfo *hash_st = NULL;
    double **coeffs ;
    uint8_t **toggles;
    unsigned int nbframes;
    unsigned int nbcoeffs;
    unsigned int buflen = env->GetArrayLength(buf);
    float *buf2 = env->GetFloatArrayElements(buf, &iscopy);

    if (P > 0){
	if (audiohash(buf2, &hash, &coeffs, &toggles, &nbcoeffs, &nbframes, NULL, NULL, buflen, P, sr, &hash_st) < 0){
	    env->ReleaseFloatArrayElements(buf, buf2, JNI_FALSE);
	    return NULL;
	}
    } else {
	if (audiohash(buf2, &hash, &coeffs, NULL, &nbcoeffs, &nbframes, NULL, NULL, buflen, P, sr, &hash_st) < 0){
	    env->ReleaseFloatArrayElements(buf, buf2, JNI_FALSE);
	    return NULL;
	}
    }

    jintArray hasharray = env->NewIntArray(nbframes);
    env->SetIntArrayRegion(hasharray, 0, nbframes, (int*)hash);

    jbyteArray byteArrayRow = env->NewByteArray(P);
    jobjectArray togglesArray = (jobjectArray)env->NewObjectArray(nbframes, env->GetObjectClass(byteArrayRow), 0);
    env->DeleteLocalRef(byteArrayRow);

    jdoubleArray doubleArrayRow = env->NewDoubleArray(nbcoeffs);
    jobjectArray coeffsArray = (jobjectArray)env->NewObjectArray(nbframes+2, env->GetObjectClass(doubleArrayRow), 0);
    env->DeleteLocalRef(doubleArrayRow);

    for (unsigned int i=0;i<nbframes;i++){
	if (P > 0){
	    byteArrayRow = env->NewByteArray(P);
	    env->SetByteArrayRegion(byteArrayRow, 0, P, (jbyte*)toggles[i]);
	    env->SetObjectArrayElement(togglesArray, i, byteArrayRow);
	    ph_free(toggles[i]);
	    env->DeleteLocalRef(byteArrayRow);
	}

	doubleArrayRow = env->NewDoubleArray(nbcoeffs);
	env->SetDoubleArrayRegion(doubleArrayRow, 0, nbcoeffs, (jdouble*)coeffs[i]);
	env->SetObjectArrayElement(coeffsArray, i, doubleArrayRow);
	ph_free(coeffs[i]);
	env->DeleteLocalRef(doubleArrayRow);
    }

    doubleArrayRow = env->NewDoubleArray(nbcoeffs);
    env->SetDoubleArrayRegion(doubleArrayRow, 0, nbcoeffs, (jdouble*)coeffs[nbframes]);
    ph_free(coeffs[nbframes]);
    env->DeleteLocalRef(doubleArrayRow);

    doubleArrayRow = env->NewDoubleArray(nbcoeffs);
    env->SetDoubleArrayRegion(doubleArrayRow, 0, nbcoeffs, (jdouble*)coeffs[nbframes+1]);
    ph_free(coeffs[nbframes+1]);
    env->DeleteLocalRef(doubleArrayRow);

    jclass audiohashclass = env->FindClass("org/phash/AudioHashObject");

    jmethodID ctorId = env->GetMethodID(audiohashclass, "<init>", "()V");
    jobject hashobj = env->NewObject(audiohashclass, ctorId);

    jfieldID hashfield = env->GetFieldID(audiohashclass, "hash", "[I" );
    jfieldID coeffsfield = env->GetFieldID(audiohashclass, "coeffs", "[[D");
    jfieldID togglesfield = env->GetFieldID(audiohashclass, "toggles", "[[B");

    env->SetObjectField(hashobj, hashfield,  hasharray);
    env->SetObjectField(hashobj, coeffsfield, coeffsArray);
    env->SetObjectField(hashobj, togglesfield, togglesArray);

    ph_free(hash);
    ph_free(coeffs);
    if (P > 0) ph_free(toggles);
    ph_hashst_free(hash_st);
    env->ReleaseFloatArrayElements(buf, buf2, JNI_FALSE);

    return hashobj;
}

