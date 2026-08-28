package keel

import (
	"fmt"
	"os"

	"github.com/aadit-n3rdy/keel/tree"
)

type Keel struct {
	// Keel instance pinned to a directory, a namespace of KV-stores
	dir  string
	tree *tree.Tree
}

type keelValue struct {
	val     []byte
	deleted bool
}

func NewKeel(dir string) (*Keel, error) {
	info, err := os.Stat(dir)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() {
		return nil, fmt.Errorf("Invalid directory %s", dir)
	}
	return &Keel{dir: dir}, nil
}

func (k *Keel) Get(key []byte) ([]byte, error) {
	return nil, nil
}

func (k *Keel) Set(key []byte, value []byte) error {
	return nil
}

func (k *Keel) Delete(key []byte) error {
	return nil
}
