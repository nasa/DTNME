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

#ifndef _MULTIGRAPH_H_
#define _MULTIGRAPH_H_

#include <vector>
#include <oasys/debug/InlineFormatter.h>
#include <oasys/debug/Formatter.h>
#include <oasys/debug/Logger.h>
#include <oasys/util/StringBuffer.h>
#include <oasys/util/Time.h>

namespace dtn {

class Bundle;

/**
 * Data structure to represent a multigraph.
 */
template <typename _NodeInfo, typename _EdgeInfo>
class MultiGraph : public oasys::Formatter, public oasys::Logger {
public:
    /// @{ Forward Declarations of helper classes
    class Edge;
    class EdgeVector;
    class Node;
    class NodeVector;
    class WeightFn;
    /// @}
    
    /// Constructor
    MultiGraph();

    /// Destructor
    ~MultiGraph();

    /// Add a new node
    Node* add_node(const std::string& id, const _NodeInfo& info);

    /// Find a node with the given id
    Node* find_node(const std::string& id) const;
    
    /// Delete a node and all its edges
    bool del_node(const std::string& id);
    
    /// Add an edge
    Edge* add_edge(Node* a, Node* b, const _EdgeInfo& info);

    /// Find an edge
    Edge* find_edge(const Node* a, const Node* b, const _EdgeInfo& info);
    
    /// Remove the specified edge from the given node, deleting the
    /// Edge object
    bool del_edge(Node* node, Edge* edge);
    
    /// Find the shortest path between two nodes by running Dijkstra's
    /// algorithm, filling in the edge vector with the best path
    ///
    /// Optionally also takes a bundle to route from a to b
    void shortest_path(const Node* a, const Node* b,
                       EdgeVector* path, WeightFn* weight_fn,
                       Bundle* bundle = NULL);

    /// More limited version of the shortest path that just returns
    /// the next hop edge.
    Edge* best_next_hop(const Node* a, const Node* b, WeightFn* weight_fn,
                        Bundle* bundle = NULL);

    /// Clear the contents of the graph
    void clear();

    /// Virtual from Formatter
    int format(char* buf, size_t sz) const;

    /// Return a string dump of the graph
    std::string dump() const
    {
        char buf[1024];
        int len = format(buf, sizeof(buf));
        return std::string(buf, len);
    }

    /// Accessor for the nodes array
    const NodeVector& nodes() { return nodes_; }

    /// Struct used to encapsulate state that may be needed by weight
    /// functors and which is the same for a whole shortest path
    /// computation.
    struct SearchInfo {
        SearchInfo(Bundle* bundle);

        Bundle*     bundle_;
        oasys::Time now_;
    };

    /// The abstract weight function class
    class WeightFn {
    public:
        virtual ~WeightFn() {}
        virtual u_int32_t operator()(const SearchInfo& info,
                                     const Edge* edge) = 0;
    };

    /// The node class
    class Node {
    public:
        /// Constructor
        Node(const std::string& id, const _NodeInfo info)
            : id_(id), info_(info) {}

        ~Node();

        bool del_edge(Edge* edge);
        
        const std::string& id() const { return id_; }

        const _NodeInfo& info() const { return info_; }
        _NodeInfo& mutable_info() { return info_; }

        const EdgeVector& out_edges() const { return out_edges_; }
        const EdgeVector& in_edges()  const { return in_edges_; }
        
        EdgeVector& out_edges() { return out_edges_; }
        EdgeVector& in_edges()  { return in_edges_; }
        
    private:
        friend class MultiGraph;
        
        std::string id_;
        _NodeInfo   info_;
        
        EdgeVector  out_edges_;
        EdgeVector  in_edges_;

        // XXX/demmer see below
        // friend class MultiGraph::DijkstraCompare;
        
        /// @{ Dijkstra algorithm state
        mutable u_int32_t distance_;

        mutable enum {
            WHITE, GRAY, BLACK
        } color_;
        
        mutable Edge* prev_;
        /// @} 
    };

    /// XXX/demmer this stupid helper function is needed because
    /// DijkstraCompare can't be made a friend class of Node without
    /// crashing gcc 3.4
    static u_int32_t NodeDistance(const Node* n) {
        return n->distance_;
    }

    /// Helper class to compute Dijkstra distance
    struct DijkstraCompare {
        bool operator()(const Node* a, const Node* b) const {
            return NodeDistance(a) > NodeDistance(b);
        }
    };

    /// The edge class.
    class Edge {
    public:
        /// Constructor
        Edge(Node* s, Node* d, const _EdgeInfo info)
            : source_(s), dest_(d), info_(info) {}

        /// Destructor clears contents for debugging purposes
        ~Edge() { source_ = NULL; dest_ = NULL; }

        Node*       source()       { return source_; }
        Node*       dest()         { return dest_; }
        const Node* source() const { return source_; }
        const Node* dest()   const { return dest_; }

        const _EdgeInfo& info()   const { return info_; }
        _EdgeInfo& mutable_info() { return info_; }

    protected:
        Node*     source_;
        Node*     dest_;
        _EdgeInfo info_;
    };

    typedef oasys::InlineFormatter<_EdgeInfo> EdgeFormatter;
    
    /// Helper data structure for a vector of nodes
    class NodeVector : public oasys::Formatter,
                       public std::vector<Node*> {
    public:
        int format(char* buf, size_t sz) const;
        std::string dump() const
        {
            char buf[1024];
            int len = format(buf, sizeof(buf));
            return std::string(buf, len);
        }
    };
    
    /// Helper data structure for a vector of edges
    class EdgeVector : public oasys::Formatter,
                       public std::vector<Edge*> {
    public:
        int format(char* buf, size_t sz) const;
        void debug_format(oasys::StringBuffer* buf) const;
        std::string dump() const
        {
            char buf[1024];
            int len = format(buf, sizeof(buf));
            return std::string(buf, len);
        }
    };

protected:
    /// Helper function to follow the prev_ links that result from a
    /// Dijkstra search from a to b and build an EdgeVector
    bool get_reverse_path(const Node* a, const Node* b, EdgeVector* path);
    
    /// The vector of all nodes
    NodeVector nodes_;
};

} // namespace dtn

#include "MultiGraph.tcc"

#endif /* _MULTIGRAPH_H_ */
