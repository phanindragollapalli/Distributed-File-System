/* FILE OPERATIONS - Person 2, Days 3-7 - NOT Person 1's Responsibility
 * Core file operation implementations: CREATE, READ, WRITE, APPEND, DELETE
 * CREATE: Creates new empty file on disk and updates NS metadata
 * READ: Loads entire file into memory, converts to linked list, sends to client
 * WRITE: Accepts word index and content, modifies file with sentence-level locking
 * APPEND: Adds content to end of file
 * DELETE: Removes file from disk and clears metadata
 *
 * This module is implemented by Person 2 as part of their file operations work
 * Person 1 is NOT responsible for this implementation
 */
