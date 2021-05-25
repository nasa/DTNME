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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "../debug/DebugUtils.h"
#include "../debug/Logger.h"
#include "../serialize/Serialize.h"

#include "StoreDetail.h"

#ifdef LIBODBC_ENABLED

namespace oasys {

StoreDetailItem::StoreDetailItem(	const char *		column_name,
									const detail_kind_t	column_type,
									void *				data_ptr,
								    int32_t				data_size,
								    detail_get_put_t	op ):
		column_name_(column_name), column_type_(column_type),
		data_ptr_(data_ptr), data_size_(data_size)
{
	// Point at buffer rather than supplied data_ptr for VARCHAR and BLOB types
	ASSERT(data_ptr != NULL);
	if ((op == DGP_GET) && ((column_type == DK_VARCHAR) || (column_type == DK_BLOB))) {
		data_ptr_orig_ = data_ptr;
		data_ptr_ = (void *)data_buf_;
		data_size_ = SDI_GET_BUF_SIZE;
	}
}

//----------------------------------------------------------------------
void
StoreDetailItem::set_data_size(int32_t new_size)
{
	ASSERT((new_size <= data_size_) && ((new_size >= 0) || (new_size == SDI_NULL)));
	data_size_ = new_size;
}


//----------------------------------------------------------------------
StoreDetail::StoreDetail(char			   *logclass,
						 char 			   *logpath,
						 detail_get_put_t	op):
		Logger(logclass, "%s", logpath),
		detail_vector_(), op_(op)
{
}
//----------------------------------------------------------------------
StoreDetail::~StoreDetail()
{
	detail_vector_.clear();
}

//----------------------------------------------------------------------
void
StoreDetail::serialize(SerializeAction* a)
{
	(void)a; //Not Used
	PANIC("Store Detail::Serialize entered inappropriately.");

}

//----------------------------------------------------------------------
void
StoreDetail::add_detail(const char* 		column_name,
						const detail_kind_t kind,
						void * 				data_ptr,
					    int32_t				data_size )
{
	log_debug("add_detail: column %s, kind %d, size %d %p", column_name, kind, data_size, data_ptr);
	StoreDetailItem* item = new StoreDetailItem(column_name, kind, data_ptr, data_size, op_);
	detail_vector_.push_back(item);
}

//----------------------------------------------------------------------
StoreDetail::iterator
StoreDetail::begin()
{
	return this->detail_vector_.begin();
}

//----------------------------------------------------------------------
StoreDetail::iterator
StoreDetail::end()
{
	return this->detail_vector_.end();
}

//----------------------------------------------------------------------
}  /* namespace oasys */

#endif // LIBODBC_ENABLED
