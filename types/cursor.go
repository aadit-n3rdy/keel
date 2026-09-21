package types

import "bytes"

type Result struct {
	Key   []byte
	Value []byte
}

func (res *Result) Equal(other *Result) bool {
	return bytes.Equal(res.Key, other.Key) && bytes.Equal(res.Value, other.Value)
}

type Cursor interface {
	GetNext() Result
}

var NullResult = Result{nil, nil}

type ChainedCursor struct {
	cursors []Cursor
}

func NewChainedCursor(cursors ...Cursor) ChainedCursor {
	return ChainedCursor{cursors: cursors}
}

func (cc *ChainedCursor) GetNext() Result {
	if len(cc.cursors) == 0 {
		return NullResult
	}
	nextResult := cc.cursors[0].GetNext()
	if nextResult.Equal(&NullResult) {
		cc.cursors[0] = nil
		cc.cursors = cc.cursors[1:]
	}
	return nextResult
}

type SortedChainedCursor struct {
	cursors    []Cursor
	lastResult []Result
}

func NewSortedChainedCursor(cursors ...Cursor) SortedChainedCursor {
	result := SortedChainedCursor{cursors: cursors, lastResult: make([]Result, len(cursors))}
	for i, cursor := range result.cursors {
		result.lastResult[i] = cursor.GetNext()
	}
	return result
}

func (scc *SortedChainedCursor) GetNext() Result {
	if len(scc.cursors) == 0 {
		return NullResult
	}
	i := 0
	curMin := NullResult
	curMinIndex := -1
	for i = 0; i < len(scc.cursors); i++ {
		if scc.lastResult[i].Equal(&NullResult) {
			continue
		}
		if curMin.Equal(&NullResult) || bytes.Compare(curMin.Key, scc.lastResult[i].Key) > 0 {
			curMin = scc.lastResult[i]
			curMinIndex = i
		}
	}
	scc.lastResult[curMinIndex] = scc.cursors[curMinIndex].GetNext()
	return curMin
}
