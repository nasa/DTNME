/*
 *    Copyright 2004-2006 Intel Corporation
 *    Copyright 2011 Trinity College Dublin
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _STORE_DETAIL_H_
#define _STORE_DETAIL_H_

#ifdef LIBODBC_ENABLED

namespace oasys {

/**
 * This class is used as a base class for creating auxiliary tables that can be
 * used to extract a number of fields from a SerializableObject that can be stored
 * separately in a database table to facilitate various operations. The exact fields
 * of the object to be stored are controlled by the setup list in the constructor of
 * any derived class.  The class is derived from SerializableObject in order to allow
 * it to be used in conjunction with the Oasys DurableStore mechanisms, but in practice
 * the data is *not* serialized but is stored separately in columns of the created table.
 * Hence the serialize routine will always be a dummy and should never be called.
 *
 * WARNING: [Elwyn 20120103] The 'get' case (op = DGP_GET) has not been tested as yet.
 */

// Types of database field we can use.
enum detail_kind_t {
	DK_CHAR	= 0,	///< Character fields
	DK_SHORT,		///< Signed 16 bit integer fields
	DK_USHORT,		///< Unsigned 16 bit integer fields
	DK_LONG,		///< Signed 32 bit integer fields
	DK_ULONG,		///< Unsigned 32 bit integer fields
	DK_LONG_LONG,	///< Signed 64 bit integer fields
	DK_ULONG_LONG,	///< Unsigned 64 bit integer fields
	DK_FLOAT,		///< Single precision floating point fields
	DK_DOUBLE,		///< Double precision floating point fields
	DK_DATETIME,	///< Date/Time fields
	DK_VARCHAR,		///< Variable width character fields
	DK_BLOB			///< Opaque data fields
};

enum detail_get_put_t {
	DGP_PUT,		///< Data is to be put into the database
	DGP_GET			///< Data is to be retrieved from database
};

#define SDI_GET_BUF_SIZE 2000	///< Buffer used when retrieving data from database

class StoreDetailItem
{
    public:
		StoreDetailItem(const char *			column_name,
					     const detail_kind_t	column_type,
					     void *					data_ptr,
					     int32_t				data_size,
					     detail_get_put_t		op = DGP_PUT );
		~StoreDetailItem() {};
		const char *		column_name()	{ return column_name_; }
		detail_kind_t	column_type()	{ return column_type_; }
		void *				data_ptr()		{ return data_ptr_; }
		int32_t				data_size()		{ return data_size_; }
		int32_t            *data_size_ptr() { return &data_size_; }
		void				set_data_size(int32_t new_size);

		static const int32_t SDI_NULL = (-1);				///< Used to indicate a NULL data item

    private:
		const char * 		column_name_;					///< The associated database column name
		const detail_kind_t	column_type_;					///< The data type of the column used to store item
		void *				data_ptr_;
		void *				data_ptr_orig_;					///< Pointer to the data to be read/written
		char *				data_buf_[SDI_GET_BUF_SIZE];	///< Data buffer used when getting from database
		int32_t				data_size_;						///< Size of data item

};

class StoreDetail : public oasys::Logger,
                    public oasys::SerializableObject
{
    private:
        typedef std::vector<StoreDetailItem*> Vector;

    public:
	    StoreDetail(char			   *logclass,
	    		    char 			   *logpath,
					detail_get_put_t	op = DGP_PUT );
	    ~StoreDetail();
	    void  serialize(SerializeAction* a);
	    typedef Vector::iterator iterator;
	    iterator begin();
	    iterator end();

    protected:
	    void add_detail(const char *		column_name,
	    		        const detail_kind_t	column_type,
	    		        void *				data_ptr,
	    		        int32_t				data_size);
	    Vector				detail_vector_;		///< List of details for auxiliary table.
	    detail_get_put_t	op_;				///< Whether we are putting or getting data


};


}  /* namespace oasys */
#endif  /* _STORE_DETAIL_H_ */

#endif // LIBODBC_ENABLED
