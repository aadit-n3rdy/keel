package tree

import "github.com/aadit-n3rdy/keel/types"

type Tree struct {
	root *treeNode
	size int
}

type treePair struct {
	key []byte
	val types.Value
}

func NewTree() *Tree {
	return &Tree{
		root: newTreeNode(),
		size: 0,
	}
}

func (t *Tree) Set(key []byte, value types.Value) bool {
	// returns true if size > limit
	keyOwn := make([]byte, len(key))
	copy(keyOwn, key)
	dataOwn := make([]byte, len(value.Data))
	copy(dataOwn, value.Data)
	t.root = t.root.setPair(treePair{keyOwn, types.Value{Deleted: value.Deleted, Data: dataOwn}}, &t.size)
	return t.size >= TREE_MAX_KEYS
}

func (t *Tree) Get(key []byte) (types.Value, bool) {
	res, ok := t.root.get(key)
	if ok && res.Deleted {
		return res, false
	}
	return res, ok
}

func (t *Tree) ForEach(iterFunc func(key []byte, value types.Value) error) error {
	return t.root.forEach(iterFunc)
}
