package wal

import (
	"encoding/binary"
	"encoding/hex"
	"errors"
	"io"
	"os"
	"path"

	"github.com/aadit-n3rdy/keel/tree"
	"github.com/aadit-n3rdy/keel/types"
	"github.com/aadit-n3rdy/keel/util"
)

type Wal struct {
	f *os.File
}

func NewWal(dir string) (*Wal, error) {
	f, err := os.Create(path.Join(dir, "wal.keelwal"))
	if err != nil {
		return nil, err
	}

	wal := new(Wal)
	wal.f = f
	return wal, nil
}

func (wal *Wal) WriteOp(operation *types.Operation) error {
	switch operation.OpType {
	case types.OperationRead:
		return nil
	case types.OperationWrite:
		err := binary.Write(wal.f, binary.BigEndian, operation.OpType)
		if err != nil {
			return err
		}
		err = util.WriteVarLenBytes(wal.f, operation.Key)
		if err != nil {
			return err
		}
		err = util.WriteVarLenBytes(wal.f, operation.Value)
		if err != nil {
			return err
		}
	case types.OperationDelete:
		err := binary.Write(wal.f, binary.BigEndian, operation.OpType)
		if err != nil {
			return err
		}
		err = util.WriteVarLenBytes(wal.f, operation.Key)
		if err != nil {
			return err
		}
	}
	return nil
}

func (wal *Wal) Commit() error {
	return wal.f.Sync()
}

func (wal *Wal) Close() error {
	return wal.f.Close()
}

func WalToTree(dir string) (*tree.Tree, error) {
	f, err := os.Open(path.Join(dir, "wal.keelwal"))
	if err != nil {
		return nil, err
	}
	defer f.Close()

	tree := tree.NewTree()

	for {
		var opType types.OperationEnum
		err = binary.Read(f, binary.BigEndian, &opType)
		if err == io.EOF {
			// all WAL operations stored
			return tree, nil
		}
		if err != nil {
			return tree, err
		}
		keyBuf := make([]byte, 0)
		valBuf := make([]byte, 0)
		switch opType {
		case types.OperationWrite:
			keyBuf, err = util.ReadVarLenBytes(f, keyBuf)
			if err != nil {
				return tree, err
			}

			valBuf, err = util.ReadVarLenBytes(f, valBuf)
			if err != nil {
				return tree, err
			}
			tree.Set(keyBuf, types.Value{Deleted: false, Data: valBuf})
		case types.OperationDelete:
			keyBuf, err = util.ReadVarLenBytes(f, keyBuf)
			if err != nil {
				return tree, err
			}
			tree.Set(keyBuf, types.Value{Deleted: true, Data: []byte{}})
		default:
			// TODO: wrap in error type
			return tree, errors.New("Unkown operation type found: 0x" + hex.EncodeToString([]byte{byte(opType)}))
		}
	}
}
