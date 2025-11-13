/* FILE LOCKING - Person 2, Days 5-7 - NOT Person 1's Responsibility
 * Sentence-level read-write locks for concurrent file access
 * Uses pthread read-write locks (pthread_rwlock_t) for each sentence
 * Multiple readers can access same sentence simultaneously
 * Only one writer can access a sentence at a time
 * Writers block all readers and other writers on that sentence
 * Enables concurrent writes to different sentences in same file
 *
 * This module is implemented by Person 2 as part of their file operations work
 * Person 1 is NOT responsible for this implementation
 */
