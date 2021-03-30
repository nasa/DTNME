#ifndef __FILEBACKEDOBJECT_H__
#define __FILEBACKEDOBJECT_H__

#include <memory>

#include "../thread/Mutex.h"
#include "../debug/Logger.h"

// predecl
struct stat;

namespace oasys {

class FileBackedObject;
class FileBackedObjectStore;
class SerializableObject;

typedef std::auto_ptr<FileBackedObject> FileBackedObjectHandle;

class FileBackedObject {
public:
    class Tx;
    class OpenScope;
    
    friend class FileBackedObject::Tx;
    friend class FileBackedObject::OpenScope;
    friend class FileBackedObjectStore;
    
    /*!
     * Parameters to the FileBackedObject
     */
    enum {
        INIT_BLANK = 1 << 1,  // start off with a blank file in a TX
        UNLINKED   = 1 << 8,  // status bit for unlinked file
    };
    
    /*! 
     * This implements a simple per object transaction mechanism using
     * the atomic rename provided by the filesystem.
     *
     * No more than one transaction should be active at a time. (right
     * now).
     *
     * @param flags to the FileBackedObject.
     */
    class Tx : public Logger {
    public:
        Tx(FileBackedObject* backing_file, int flags);

        /*!
         * Destructor commits the transaction if it already hasn't
         * been committed.
         */
        ~Tx();
        
        /*!
         * @return The FileObject that is going to be committed. const
         * because all of the operations should go through the
         * transaction.
         */
        FileBackedObject* object();

        /*!
         * Commit the transaction.
         */
        void commit();

        /*!
         * Abort the transaction.
         */
        void abort();
        
    private:
        FileBackedObject* original_file_;
        FileBackedObject* tx_file_;
    };
    
    /*!
     * Use open scope if you want to keep a FileBackedObject fd open
     * for the duration of the scope for performance.
     */
    class OpenScope {
    public:
        OpenScope(FileBackedObject* obj);
        ~OpenScope();
        
    private:
        FileBackedObject* obj_;
    };
    
    /*!
     * Use TxHandle to manage the Tx.
     */
    typedef std::auto_ptr<Tx> TxHandle;

    /*!
     * Opens file on construction.
     *
     * @param filename Name of the backing file.
     * @param flags    Flags for behavior (see above).
     */
    FileBackedObject(const std::string& filename, int flags);    

    /*!
     * Closes file on destruction.
     */
    ~FileBackedObject();

    /*!
     * @return A new transaction on the file.
     */
    TxHandle start_tx(int flags);
    
    /*!
     * Set the flags of the file.
     */
    void set_flags(int flags);
    
    /*!
     * @param stat_buf Stats maintained by the file system.
     */
    void get_stats(struct stat* stat_buf) const;

    /*!
     * Return the size in bytes of the file.
     */
    size_t size() const;

    /*!
     * This only sets the following fields:
     *   uid, gid, mode, times
     */
    void set_stats(struct stat* stat_buf);

    /*!
     * @return Number of bytes read.
     */
    size_t read_bytes(size_t offset, u_char* buf, size_t length) const;

    /*!
     * Will extend the length of the file if needed.
     * 
     * @return Number of bytes written.
     */
    size_t write_bytes(size_t offset, const u_char* buf, size_t length);

    /*!
     * Append bytes to the file.
     *
     * @return Number of bytes written.
     */
    size_t append_bytes(const u_char* buf, size_t length);

    /**
     * Replace the underlying file with a hard link to the given path
     * or a copy of the file contents if a link can't be created.
     */
    bool replace_with_file(const std::string& filename);
    
    /*!
     * Make the file size.
     */
    void truncate(size_t size);

    /*!
     * FSync the data in fd.
     */
    void fsync_data();

    /*!
     * @return Name of the backing file for the object.
     */
    const std::string& filename() const { return filename_; }

    /*!
     * @param offset negative values mean start at the end.
     * @return Error code from StreamSerialize.
     */
    int serialize(const SerializableObject* obj, int offset = 0);

    /*!
     * @return Error code from StreamUnserialize.
     */
    int unserialize(SerializableObject* obj);
    
private:
    std::string filename_;
    mutable int fd_;
    int         flags_;

    oasys::Mutex lock_;

    /*!
     * Offset into the file, maintained to optimize out a call to seek.
     */
    mutable size_t cur_offset_;
    mutable int    open_count_;
    
    /*!
     * Open the file if needed and according to the flags.
     */
    void open() const;
    
    /*!
     * Close the file if needed and according to the flags.
     */
    void close() const;
    
    /*!
     * Delete the file from the filesystem.
     */
    void unlink();
    
    /*!
     * Reload the fd from the file.
     */
    void reload();
};

} // namespace oasys

#endif /* __FILEBACKEDOBJECT_H__ */
