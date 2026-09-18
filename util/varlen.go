package util

import (
	"encoding/binary"
	"fmt"
	"io"
	"unsafe"
)

type varlensize int64

func GetVarLenBytesSize(f io.Reader) (varlensize, error) {
	// Function to convert the next 4 bytes from `f` into an int32, to allocate sizes for the buffer coming after
	size := varlensize(0)
	lenbuf := make([]byte, unsafe.Sizeof(size))

	var err error

	_, err = io.ReadFull(f, lenbuf[:])
	if err != nil {
		return 0, fmt.Errorf("failed to get var len bytes size: %w", err)
	}
	_, err = binary.Decode(lenbuf[:], binary.BigEndian, &size)
	if err != nil {
		return 0, err
	}
	return size, err
}

func WriteVarLenBytes(f io.Writer, data []byte) error {
	err := binary.Write(f, binary.BigEndian, varlensize(len(data)))
	if err != nil {
		return err
	}
	_, err = f.Write(data)
	return err
}

func ReadVarLenBytes(f io.Reader, data []byte) ([]byte, error) {
	size, err := GetVarLenBytesSize(f)
	if err != nil {
		return nil, err
	}
	if data == nil {
		data = make([]byte, int(size))
	} else if len(data) < int(size) {
		data = append(data, make([]byte, int(size)-len(data))...)
	} else {
		data = data[:size]
	}
	_, err = io.ReadFull(f, data)
	if err != nil {
		return nil, err
	}
	return data, nil
}

func SkipVarLenBytes(f io.ReadSeeker) error {
	size, err := GetVarLenBytesSize(f)
	if err != nil {
		return err
	}
	_, err = f.Seek(int64(size), io.SeekCurrent)
	if err != nil {
		return err
	}
	return nil
}
