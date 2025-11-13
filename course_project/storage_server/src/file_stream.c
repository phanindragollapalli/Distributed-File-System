/* FILE STREAM - Person 2, Days 11-12 - NOT Person 1's Responsibility
 * Stream file contents word-by-word to client with 0.1 second delay
 * Client connects directly to Storage Server for streaming (not through NS)
 * Loads file, parses into words, sends one word at a time
 * Uses usleep(100000) for 0.1 second delay between words
 * Must handle graceful disconnection if Storage Server goes down mid-stream
 *
 * This module is implemented by Person 2 as part of their file operations work
 * Person 1 is NOT responsible for this implementation
 */
