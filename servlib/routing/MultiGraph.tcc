/*
 *    Copyright 2007 Intel Corporation
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

#include "MultiGraph.h"
#include <oasys/util/StringAppender.h>
#include <oasys/util/UpdatablePriorityQueue.h>

namespace dtn {

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline MultiGraph<_NodeInfo,_EdgeInfo>
::MultiGraph()
    : Logger("MultiGraph", "/dtn/route/graph")
{
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline MultiGraph<_NodeInfo,_EdgeInfo>
::~MultiGraph()
{
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Node*
MultiGraph<_NodeInfo,_EdgeInfo>
::add_node(const std::string& id, const _NodeInfo& info)
{
    Node* n = new Node(id, info);
    this->nodes_.push_back(n);
    return n;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Node*
MultiGraph<_NodeInfo,_EdgeInfo>
::find_node(const std::string& id) const
{
    typename NodeVector::const_iterator iter;
    for (iter = this->nodes_.begin(); iter != this->nodes_.end(); ++iter) {
        if ((*iter)->id_ == id) {
            return *iter;
        }
    }
    return NULL;
}
    
//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline bool
MultiGraph<_NodeInfo,_EdgeInfo>
::del_node(const std::string& id)
{
    class NodeVector::iterator iter;
    for (iter = this->nodes_.begin(); iter != this->nodes_.end(); ++iter) {
        if ((*iter)->id_ == id) {
            delete (*iter);
            this->nodes_.erase(iter);
            return true;
        }
    }

    return false;
}
    
//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline void
MultiGraph<_NodeInfo,_EdgeInfo>
::clear()
{
    class NodeVector::iterator iter;
    for (iter = this->nodes_.begin(); iter != this->nodes_.end(); ++iter) {
        delete (*iter);
    }

    this->nodes_.clear();
}
    
//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Edge*
MultiGraph<_NodeInfo,_EdgeInfo>
::add_edge(Node* a, Node* b, const _EdgeInfo& info)
{
    Edge* e = new Edge(a, b, info);
    a->out_edges_.push_back(e);
    b->in_edges_.push_back(e);
    return e;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Edge*
MultiGraph<_NodeInfo,_EdgeInfo>
::find_edge(const Node* a, const Node* b, const _EdgeInfo& info)
{
    typename EdgeVector::const_iterator iter;
    for (iter = a->out_edges_.begin(); iter != a->out_edges_.end(); ++iter) {
        const Edge* edge = *iter;
        if ((edge->dest() == b) && (edge->info() == info))
        {
            return *iter;
        }
    }

    return NULL;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline bool
MultiGraph<_NodeInfo,_EdgeInfo>
::del_edge(Node* node, Edge* edge)
{
    return node->del_edge(edge);
}


//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline
MultiGraph<_NodeInfo,_EdgeInfo>
::Node::~Node()
{
    while (! this->out_edges_.empty()) {
        this->del_edge(*this->out_edges_.begin());
    }

    while (! this->in_edges_.empty()) {
        Edge* edge = *this->in_edges_.begin();
        edge->source()->del_edge(edge);
    }
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline bool
MultiGraph<_NodeInfo,_EdgeInfo>
::Node::del_edge(Edge* edge)
{
    typename EdgeVector::iterator ei, ei2;
    for (ei = this->out_edges_.begin();
         ei != this->out_edges_.end(); ++ei)
    {
        if (*ei == edge)
        {
            // delete the corresponding in_edge pointer, which must
            // exist or else there's some internal inconsistency
            ei2 = edge->dest()->in_edges_.begin();
            while (1) {
                ASSERTF(ei2 != edge->dest()->in_edges_.end(),
                        "out_edge / in_edge mismatch!!");
                if (*ei2 == edge)
                    break;
                ++ei2;
            }
            
            this->out_edges_.erase(ei);
            edge->dest()->in_edges_.erase(ei2);
            delete edge;
            return true;
        }
    }
    
    return false;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline
MultiGraph<_NodeInfo,_EdgeInfo>
::SearchInfo::SearchInfo(Bundle* bundle)
    : bundle_(bundle)
{
    now_.get_time();
}
        
//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline bool
MultiGraph<_NodeInfo,_EdgeInfo>
::get_reverse_path(const Node* a, const Node* b, EdgeVector* path)
{
    const Node* cur = b;
    do {
        ASSERT(cur->prev_->dest() == cur);
        ASSERT(cur->prev_->source() != cur);

        path->push_back(cur->prev_);
        cur = cur->prev_->source();
    } while (cur != a);

    return true;
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline void
MultiGraph<_NodeInfo,_EdgeInfo>
::shortest_path(const Node* a, const Node* b,
                EdgeVector* path, WeightFn* weight_fn,
                Bundle* bundle)
{
    log_debug("calculating shortest path from %s -> %s",
              a->id_.c_str(), b->id_.c_str());
    
    ASSERT(a != NULL);
    ASSERT(b != NULL);
    ASSERT(path != NULL);
    ASSERT(a != b);
    path->clear();

    // cons up the search info
    SearchInfo info(bundle);
    
    // first clear the existing distances
    for (typename NodeVector::iterator i = this->nodes_.begin();
         i != this->nodes_.end(); ++i)
    {
        (*i)->distance_ = 0xffffffff;
        (*i)->color_    = Node::WHITE;
        (*i)->prev_     = NULL;
    }
    
    // compute dijkstra distances
    oasys::UpdatablePriorityQueue<const Node*,
        std::vector<const Node*>,
        DijkstraCompare> q;
    
    a->distance_ = 0;
    q.push(a);
    do {
        const Node* cur = q.top();
        q.pop();

        for (typename EdgeVector::const_iterator ei = cur->out_edges_.begin();
             ei != cur->out_edges_.end(); ++ei)
        {
            Edge* edge = *ei;
            Node* peer = edge->dest();

            ASSERT(edge->source() == cur);
            ASSERT(peer != cur); // no loops
            
            u_int32_t weight = (*weight_fn)(info, edge);

            log_debug("examining edge id %s (weight %u) "
                      "from %s (distance %u) -> %s (distance %u)",
                      EdgeFormatter().format(edge->info()),
                      weight, cur->id_.c_str(), cur->distance_,
                      peer->id_.c_str(), peer->distance_);
            
            if (weight != 0xffffffff &&
                peer->distance_ > cur->distance_ + weight)
            {
                if (peer->color_ == Node::BLACK)
                {
                    log_crit("revisiting black node when "
                             "calculating shortest path from %s -> %s!!!",
                             a->id_.c_str(), b->id_.c_str());
                    
                    log_crit("cur %s: distance %u edge %s weight %u "
                             "prev edge %s from %s",
                             cur->id_.c_str(), cur->distance_,
                             EdgeFormatter().format(edge->info()),
                             weight,
                             cur->prev_
                               ? EdgeFormatter().format(cur->prev_->info())
                               : "NULL!",
                             cur->prev_ ?
                             cur->prev_->source()->id_.c_str() : "");

                    log_crit("peer %s: distance %u, prev edge %s from %s",
                             peer->id_.c_str(), peer->distance_,
                             peer->prev_
                               ? EdgeFormatter().format(peer->prev_->info())
                               : "NULL!",
                             peer->prev_ ?
                               peer->prev_->source()->id_.c_str() : "");

                    continue;
                }
                    
                peer->distance_ = cur->distance_ + weight;
                peer->prev_     = edge;

                if (peer->color_ == Node::WHITE)
                {
                    log_debug("pushing node %s (distance %u) onto queue",
                              peer->id_.c_str(), peer->distance_);
                    peer->color_ = Node::GRAY;
                    q.push(peer);
                }
                else if (peer->color_ == Node::GRAY)
                {
                    log_debug("updating node %s in queue", peer->id_.c_str());
                    q.update(peer);
                }
                else {
                    NOTREACHED;
                }
            }
        }

        cur->color_ = Node::BLACK;

        // safe to bail once we reach the destination
        if (cur == b) {
            break;
        }
             
    } while (!q.empty());
    
    if (b->distance_ == 0xffffffff) {
        log_debug("no path found from %s -> %s",
                  a->id_.c_str(), b->id_.c_str());
        return; // no path
    }

    get_reverse_path(a, b, path);
    
    size_t len = path->size();
    log_debug("found path of length %zu from %s -> %s",
              len, a->id_.c_str(), b->id_.c_str());
    for (size_t i = 0; i < len; ++i) {
        Edge* e = (*path)[len - i - 1];
        (void)e;
        log_debug("hop %zu: %s -> %s %s", i,
                  e->source()->id_.c_str(), e->dest()->id_.c_str(), 
                  EdgeFormatter().format(e->info()));
    }
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline class MultiGraph<_NodeInfo,_EdgeInfo>::Edge*
MultiGraph<_NodeInfo,_EdgeInfo>
::best_next_hop(const Node* a, const Node* b, WeightFn* weight_fn,
                Bundle* bundle)
{
    EdgeVector path;
    shortest_path(a, b, &path, weight_fn, bundle);
    if (path.empty()) {
        return NULL;
    }

    ASSERT(path.back()->source() == a); // sanity
    return path.back();
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline int
MultiGraph<_NodeInfo,_EdgeInfo>
::format(char* buf, size_t sz) const
{
    oasys::StringAppender sa(buf, sz);
    
    typename NodeVector::const_iterator ni;
    typename EdgeVector::const_iterator ei;
    
    for (ni = this->nodes_.begin(); ni != this->nodes_.end(); ++ni)
    {
        sa.appendf("%s ->", (*ni)->id_.c_str());
        
        for (ei = (*ni)->out_edges_.begin();
             ei != (*ni)->out_edges_.end(); ++ei)
        {
            sa.appendf(" %s(%s)", 
                       (*ei)->dest()->id_.c_str(),
                       EdgeFormatter().format((*ei)->info()));
        }
        sa.append("\n");
    }

    return sa.desired_length();
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline int
MultiGraph<_NodeInfo,_EdgeInfo>
::NodeVector::format(char* buf, size_t sz) const
{
    oasys::StringAppender sa(buf, sz);
    typename NodeVector::const_iterator i;
    for (i = this->begin(); i != this->end(); ++i)
    {
        sa.appendf("%s%s",
                   i == this->begin() ? "" : " ",
                   (*i)->id().c_str());
    }
    return sa.desired_length();
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline int
MultiGraph<_NodeInfo,_EdgeInfo>
::EdgeVector::format(char* buf, size_t sz) const
{
    oasys::StringAppender sa(buf, sz);
    typename EdgeVector::const_iterator i;
    for (i = this->begin(); i != this->end(); ++i)
    {
        sa.appendf("%s[%s -> %s(%s)]",
                   i == this->begin() ? "" : " ",
                   (*i)->source()->id().c_str(),
                   (*i)->dest()->id().c_str(),
                   EdgeFormatter().format((*i)->info()));
    }
    return sa.desired_length();
}

//----------------------------------------------------------------------
template <typename _NodeInfo, typename _EdgeInfo>
inline void
MultiGraph<_NodeInfo,_EdgeInfo>
::EdgeVector::debug_format(oasys::StringBuffer* buf) const
{
    typename EdgeVector::const_iterator i;
    for (i = this->begin(); i != this->end(); ++i)
    {
        buf->appendf("\t%s -> %s (%s) (distance %u)\n",
                     (*i)->source()->id().c_str(),
                     (*i)->dest()->id().c_str(),
                     EdgeFormatter().format((*i)->info()),
                     MultiGraph<_NodeInfo,_EdgeInfo>::NodeDistance((*i)->dest()));
    }
}


} // namespace dtn

