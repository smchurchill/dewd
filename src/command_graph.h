/*
 * command_tree.h
 *
 *  Created on: Dec 16, 2015
 *      Author: schurchill
 */

#ifndef COMMAND_GRAPH_H_
#define COMMAND_GRAPH_H_

namespace dew {

class node : public enable_shared_from_this<node> {
public:
	node(){}
	node(map<string,nodep> children_in) : children(children_in) {}
	node(node_fn fn_in) : fn(fn_in) {}
	node(map<string,nodep> children_in, node_fn fn_in) :
		children(children_in), fn(fn_in) {}

	void spawn(pair<string,nodep>);
	void spawn(string, nodep);
	void spawn(map<string,nodep>);
	void purge();

	void set_fn(std::function<void(nsp)> fn_in) { fn = fn_in; }
	const shared_ptr<node>& child(const string & key) const { return children.at(key); }
	map<string,nodep>::const_iterator get_child(string& key) const{ return children.find(key); }
	map<string,nodep>::const_iterator get_end() const{ return children.cend(); }
	string descendants(const int) const;

	void operator()(nsp) const;

	void own() { owned = true; }
	bool is_owned() { return owned; }
	bool is_leaf() { return children.empty(); }


private:
	map<string,nodep> children;
	node_fn fn;
	bool owned = false;
};



} //dew namespace

#endif /* COMMAND_GRAPH_H_ */
