/*
**
** Copyright 2009, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define TAG "NativeBinaryDictionary"
#include <android/log.h>

#include <jni.h>

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "dictionary.h"

// ----------------------------------------------------------------------------

using namespace s9;

static jfieldID sDescriptorField;

//
// helper function to throw an exception
//
static void throwException(JNIEnv *env, const char* ex, const char* fmt, int data)
{
    if (jclass cls = env->FindClass(ex)) {
        char msg[1000];
        sprintf(msg, fmt, data);
        env->ThrowNew(cls, msg);
        env->DeleteLocalRef(cls);
    }
}

static jint s9_BinaryDictionary_open
        (JNIEnv *env, jobject object, jobject fileDescriptor,
         jlong offset, jlong length,
         jint typedLetterMultiplier, jint fullWordMultiplier)
{
    jint fd = env->GetIntField(fileDescriptor, sDescriptorField);

    unsigned char *dict = new unsigned char[length];
    if (dict == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
            "DICT: Failed to allocate dictionary buffer");
        return 0;
    }

    lseek(fd, offset, SEEK_SET);
    size_t bytesLeft = length;
    unsigned char *p = dict;
    while (bytesLeft > 0) {
        size_t bytesRead = read(fd, p, bytesLeft);
        p += bytesRead;
        bytesLeft -= bytesRead;
    }

    Dictionary *dictionary = new Dictionary(dict, typedLetterMultiplier, fullWordMultiplier);

    return (jint) dictionary;
}

static int s9_BinaryDictionary_getSuggestions(
        JNIEnv *env, jobject object, jint dict, jintArray inputArray, jint arraySize,
        jcharArray outputArray, jintArray frequencyArray, jint maxWordLength, jint maxWords,
        jint maxAlternatives, jint skipPos)
{
    Dictionary *dictionary = (Dictionary*) dict;
    if (dictionary == NULL)
        return 0;

    int *frequencies = env->GetIntArrayElements(frequencyArray, NULL);
    int *inputCodes = env->GetIntArrayElements(inputArray, NULL);
    jchar *outputChars = env->GetCharArrayElements(outputArray, NULL);

    int count = dictionary->getSuggestions(inputCodes, arraySize, (unsigned short*) outputChars, frequencies,
            maxWordLength, maxWords, maxAlternatives, skipPos);
    
    env->ReleaseIntArrayElements(frequencyArray, frequencies, JNI_COMMIT);
    env->ReleaseIntArrayElements(inputArray, inputCodes, JNI_ABORT);
    env->ReleaseCharArrayElements(outputArray, outputChars, JNI_COMMIT);
    
    return count;
}

static jboolean s9_BinaryDictionary_isValidWord
        (JNIEnv *env, jobject object, jint dict, jcharArray wordArray, jint wordLength)
{
    Dictionary *dictionary = (Dictionary*) dict;
    if (dictionary == NULL) return (jboolean) false;

    jchar *word = env->GetCharArrayElements(wordArray, NULL);
    jboolean result = dictionary->isValidWord((unsigned short*) word, wordLength);
    env->ReleaseCharArrayElements(wordArray, word, JNI_ABORT);

    return result;
}

static void s9_BinaryDictionary_close
        (JNIEnv *env, jobject object, jint dict)
{
    Dictionary *dictionary = (Dictionary*) dict;
    delete dictionary->getDictBuffer();
    delete (Dictionary*) dict;
}

// ----------------------------------------------------------------------------

static JNINativeMethod gMethods[] = {
    {"openNative",           "(Ljava/io/FileDescriptor;JJII)I",
                                          (void*)s9_BinaryDictionary_open},
    {"closeNative",          "(I)V",            (void*)s9_BinaryDictionary_close},
    {"getSuggestionsNative", "(I[II[C[IIIII)I",  (void*)s9_BinaryDictionary_getSuggestions},
    {"isValidWordNative",    "(I[CI)Z",         (void*)s9_BinaryDictionary_isValidWord}
};

static int registerNativeMethods(JNIEnv* env, const char* className,
    JNINativeMethod* gMethods, int numMethods)
{
    jclass clazz;

    clazz = env->FindClass(className);
    if (clazz == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
            "Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
            "RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

static int registerNatives(JNIEnv *env)
{
    const char* const kClassPathName = "com/gilbertl/s9/BinaryDictionary";
    jclass clazz;

    clazz = env->FindClass("java/io/FileDescriptor");
    if (clazz == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
            "Can't find %s", "java/io/FileDescriptor");
        return -1;
    }
    sDescriptorField = env->GetFieldID(clazz, "descriptor", "I");

    return registerNativeMethods(env,
            kClassPathName, gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
}

/*
 * Returns the JNI version on success, -1 on failure.
 */
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;
    jint result = -1;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
            "ERROR: GetEnv failed");
        goto bail;
    }
    assert(env != NULL);

    if (!registerNatives(env)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
            "ERROR: native registration failed");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_4;

bail:
    return result;
}
