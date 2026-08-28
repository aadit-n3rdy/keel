package types

type Operation struct {
	OpType OperationEnum
	Key    []byte
	Value  []byte
}

type OperationEnum int8

const (
	OperationWrite = iota
	OperationRead
	OperationDelete
)
