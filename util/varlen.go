package util

import (
	"encoding/binary"
	"fmt"
	"io"
	"os"
)

func ReadVarLenBytes(f *os.File, ignore bool, offset *int64) ([]byte, error) {
	// reads 4 bytes of length, and then the actual value.
	// returns the value read, and an error. if ignore is set to true, discards the value and simply seeks
	lenbuf := [4]byte{}
	var lenRead int
	var err error
	if offset == nil {
		lenRead, err = f.Read(lenbuf[:])
	} else {
		lenRead, err = f.ReadAt(lenbuf[:], *offset)
		*offset += 4
	}
	if err != nil {
		return nil, err
	}
	if lenRead != 4 {
		return nil, fmt.Errorf("unexpected EOF as not enough length bytes")
	}
	var lenVal int32
	_, err = binary.Decode(lenbuf[:], binary.BigEndian, &lenVal)
	if err != nil {
		return nil, err
	}
	// fmt.Printf("len: %d\n", lenVal)
	if ignore {
		if offset == nil {
			_, err := f.Seek(int64(lenVal), io.SeekCurrent)
			if err != nil {
				return nil, err
			}
			// fmt.Printf("Position after ignore seek: %d\n", n)
		} else {
			*offset += int64(lenVal)
		}
		return nil, nil
	}
	keyBuf := make([]byte, lenVal)
	var keyRead int
	if offset == nil {
		keyRead, err = f.Read(keyBuf)
	} else {
		keyRead, err = f.ReadAt(keyBuf, *offset)
		*offset += int64(lenVal)
	}
	// fmt.Printf("data: %s\n", hex.EncodeToString(keyBuf))
	if err != nil {
		return nil, err
	}
	if keyRead != int(lenVal) {
		return nil, fmt.Errorf("could not read the expected number of bytes: expected %d, got %d", lenVal, keyRead)
	}
	return keyBuf, nil
}

func WriteVarLenBytes(f *os.File, data []byte) error {
	err := binary.Write(f, binary.BigEndian, int32(len(data)))
	if err != nil {
		return err
	}
	_, err = f.Write(data)
	return err
}
