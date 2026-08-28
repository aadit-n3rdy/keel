package tree

import (
	"bytes"
	"fmt"

	"github.com/aadit-n3rdy/keel/types"
)

type treeNode struct {
	left   *treeNode
	right  *treeNode
	pairs  []treePair
	bal    int
	height int
}

func newTreeNode() *treeNode {
	return &treeNode{
		left:  nil,
		right: nil,
		pairs: make([]treePair, 0, CHUNK_MAX_SIZE),
		bal:   0,
	}
}

func (tn *treeNode) setPair(tp treePair, size *int) *treeNode {
	// insert a key value pair, and return the new root
	if tn.left != nil && bytes.Compare(tn.pairs[0].key, tp.key) > 0 {
		newLeft := tn.left.setPair(tp, size)
		tn.left = newLeft
		return tn.rebalance()
	} else if tn.right != nil && bytes.Compare(tp.key, tn.pairs[len(tn.pairs)-1].key) > 0 {
		newRight := tn.right.setPair(tp, size)
		tn.right = newRight
		return tn.rebalance()
	}

	// tp.key is within current node's range
	done := false
	for i, pair := range tn.pairs {
		if bytes.Equal(pair.key, tp.key) {
			tn.pairs[i].val = tp.val
			done = true
			break
		}
	}
	if done {
		return tn
	}

	if len(tn.pairs) < CHUNK_MAX_SIZE {
		tn.pairs = append(tn.pairs, tp)
		bubsort(tn.pairs)
		*size += 1
		return tn
	}

	newNode := newTreeNode()
	splitIndex := int(CHUNK_MAX_SIZE / 2)
	midKey := tn.pairs[splitIndex].key
	for i := splitIndex; i < CHUNK_MAX_SIZE; i++ {
		newNode.pairs = append(newNode.pairs, tn.pairs[i])
	}
	tn.pairs = tn.pairs[:splitIndex]
	tn.right = tn.right.insertNode(newNode)
	if bytes.Compare(tp.key, midKey) >= 0 {
		newNode.setPair(tp, size)
	} else {
		tn.setPair(tp, size)
	}
	return tn.rebalance()
}

func (tn *treeNode) insertNode(other *treeNode) *treeNode {
	if tn == nil {
		return other
	}
	if bytes.Compare(tn.pairs[0].key, other.pairs[len(other.pairs)-1].key) > 0 {
		tn.left = tn.left.insertNode(other)
	} else if bytes.Compare(other.pairs[0].key, tn.pairs[len(tn.pairs)-1].key) > 0 {
		tn.right = tn.right.insertNode(other)
	} else {
		fmt.Printf("ERROR: insertNode called with overlapping ranges")
		return nil
	}
	return tn.rebalance()
}

func (tn *treeNode) get(key []byte) (types.Value, bool) {
	if tn == nil {
		return types.Value{}, false
	}
	if bytes.Compare(tn.pairs[0].key, key) > 0 {
		return tn.left.get(key)
	} else if bytes.Compare(key, tn.pairs[len(tn.pairs)-1].key) > 0 {
		return tn.right.get(key)
	} else {
		return binSearch(tn.pairs, key)
	}
}

func binSearch(pairs []treePair, key []byte) (types.Value, bool) {
	if len(pairs) == 0 {
		return types.Value{}, false
	}
	if len(pairs) == 1 {
		if bytes.Equal(pairs[0].key, key) {
			return pairs[0].val, true
		} else {
			return types.Value{}, false
		}
	}
	mid := len(pairs) / 2
	if bytes.Compare(key, pairs[mid].key) >= 0 {
		return binSearch(pairs[mid:], key)
	} else {
		return binSearch(pairs[:mid], key)
	}
}

func (tn *treeNode) rebalance() *treeNode {
	// recalculate height and rebalance.
	bal := tn.getBal()
	if bal > 1 {
		// right child is new parent
		// left child remains same
		// new right is right->left
		newParent := tn.right
		newRight := tn.right.left

		newParent.left = tn
		tn.right = newRight
		tn.getBal()
		newParent.getBal()
		return newParent
	} else if bal < -1 {
		// left child is new parent
		// right child remains same
		// new left is left->right
		newParent := tn.left
		newLeft := tn.left.right

		newParent.right = tn
		tn.left = newLeft
		tn.getBal()
		newParent.getBal()
		return newParent
	}
	return tn
}

func (tn *treeNode) getBal() int {
	lh := -1
	rh := -1
	if tn.left != nil {
		lh = tn.left.height
	}
	if tn.right != nil {
		rh = tn.right.height
	}
	tn.height = max(lh, rh) + 1
	return rh - lh
}

func bubsort(pairs []treePair) {
	for i := len(pairs) - 1; i > 0; i-- {
		if bytes.Compare(pairs[i].key, pairs[i-1].key) > 0 {
			return
		}
		tmp := pairs[i]
		pairs[i] = pairs[i-1]
		pairs[i-1] = tmp
	}
}

func (tn *treeNode) forEach(iterFunc func(key []byte, val types.Value) error) error {
	if tn.left != nil {
		err := tn.left.forEach(iterFunc)
		if err != nil {
			return err
		}
	}
	for _, pair := range tn.pairs {
		err := iterFunc(pair.key, pair.val)
		if err != nil {
			return err
		}
	}
	if tn.right != nil {
		err := tn.right.forEach(iterFunc)
		if err != nil {
			return err
		}
	}
	return nil
}
