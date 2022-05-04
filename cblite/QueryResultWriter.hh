//
// QueryResultWriter.hh
//
// Copyright © 2022 Couchbase. All rights reserved.
//

#pragma once
#include "LiteCoreTool.hh"
#include "fleece/Mutable.hh"

using namespace std;
using namespace fleece;


/** Abstract class that writes the result rows of a query.
    Used by QueryCommand and RemoteQueryCommand. */
class QueryResultWriter {
public:
    /// Constructor. Given the parent tool instance and an array of column name strings.
    QueryResultWriter(LiteCoreTool &tool, vector<string> columnNames)
    :tool(tool)
    ,_columnNames(move(columnNames))
    { }

    virtual ~QueryResultWriter() = default;

    /// Adds a result row. Each array item is a column value.
    /// Subclasses should call the inherited method first.
    virtual void addRow(Array row) {
        Array::iterator i(row);
        addRow(i);
    }

    /// Adds a result row. Each array item is a column value.
    /// Subclasses should call the inherited method first, or increment `_rowNumber`.
    virtual void addRow(Array::iterator &iRow) {
        ++_rowNumber;
    }

    /// Called after all the rows have been added.
    virtual void end()                      { }

    /// The current row number (from 1), or the number of rows that have been added.
    unsigned rowNumber() const              {return _rowNumber;}

protected:

    /// The width in terminal characters of a value's texxt representation.
    /// (Understands UTF-8 and recognizes double-width characters, e.g. kanji and some emoji.)
    unsigned valueWidth(Value val) {
        switch (val.type()) {
            case kFLUndefined: return 0;
            case kFLString:    return Tool::terminalStringWidth(val.asString());
            default:           return Tool::terminalStringWidth(val.toJSON(tool._json5, true));
        }
    }

    /// Textual representation of a value.
    /// Strings are written unescaped, undefined is nothing, and other types are written as JSON.
    alloc_slice valueRepresentation(Value val) {
        switch (val.type()) {
            case kFLUndefined: return nullslice;
            case kFLString:    return alloc_slice(val.asString());
            default:           return val.toJSON(tool._json5, true);
        }
    }

    LiteCoreTool&       tool;
    vector<string>      _columnNames;
    unsigned            _rowNumber = 0;
};


/** Writes query results in JSON form as an array of dicts.
    Each row's dict keys are the column names. */
class QueryResultJSONWriter : public QueryResultWriter {
public:
    QueryResultJSONWriter(LiteCoreTool &cmd, vector<string> cols)
    :QueryResultWriter(cmd, cols) {
        // Turn the column names into quoted JSON strings:
        for (string &colName : _columnNames) {
            if (!cmd._json5 || !cmd.canBeUnquotedJSON5Key(colName))
                colName = cmd.quoteJSONString(colName);
        }
    }

    void addRow(Array::iterator &iRow) override {
        QueryResultWriter::addRow(iRow);
        if (_rowNumber == 1)
            cout << "[\n  ";
        else
            cout << ",\n  ";
        if (_columnNames.size() == 1) {
            // If there's only one column, just write its value as-is:
            tool.rawPrint(iRow.value(), nullslice);
        } else {
            // Else write the row as a dict by using the column titles as keys:
            cout << "{";
            int col = 0;
            for (; iRow; ++iRow, ++col) {
                if (col > 0)
                    cout << ", ";
                cout << _columnNames[col] << ": ";
                tool.rawPrint(iRow.value(), nullslice);
            }
            cout << "}";
        }
    }

    void end() override {
        if (_rowNumber >= 1)
            cout << "]\n";
    }
};


/** Writes query results as a monospaced table, with the column names as headers.
    Because it has to compute column widths, no output is written until `end` is called. */
class QueryResultTableWriter : public QueryResultWriter {
public:
    QueryResultTableWriter(LiteCoreTool &cmd, vector<string> colNames)
    :QueryResultWriter(cmd, colNames)
    ,_numColumns(colNames.size())
    ,_rows(MutableArray::newArray())
    {
        for (string &name : _columnNames)
            _columnWidths.push_back(Tool::terminalStringWidth(name));
    }

    void addRow(Array row) override {
        ++_rowNumber;
        // Buffer the row, and write it at the end, when the column widths are known.
        //OPT: It would probably be more efficient to convert each column to a string now.
        _rows.append(row);

        if (_numColumns > 1 || !row[0].asDict()) {
            size_t col = 0;
            for (Array::iterator i(row); i; ++i, ++col)
                _columnWidths[col] = max(_columnWidths[col], valueWidth(i.value()));
        }
    }

    void addRow(Array::iterator &iRow) override {
        MutableArray row = MutableArray::newArray();
        for (; iRow; ++iRow)
            row.append(iRow.value());
        addRow(row);
    }

    void end() override {
        if (_numColumns == 1 && _rows[0].asArray()[0].asDict()) {
            writeUnnested();
            return;
        }
        if (_numColumns > 1)
            writeColumnTitles();
        for (Array::iterator irow(_rows); irow; ++irow) {
            size_t col = 0;
            for (Array::iterator i(irow->asArray()); i; ++i, ++col)
                writeValue(col, i.value());
            cout << "\n";
        }
    }

    // Given a single-column result, where each column value is a dict,
    // write it as though every unique key were a separate column.
    void writeUnnested() {
        // Scan the values to collect column names and widths:
        map<slice,unsigned> columnWidths;
        for (Array::iterator row(_rows); row; ++row) {
            Dict rowDict = row->asArray()[0].asDict();
            for (Dict::iterator i(rowDict); i; ++i) {
                auto &colWidth = columnWidths[i.keyString()];
                colWidth = max(colWidth, valueWidth(i.value()));
            }
        }
        // Column's width needs to take its own name into account:
        _numColumns = columnWidths.size();
        _columnNames.resize(_numColumns);
        _columnWidths.resize(_numColumns);
        size_t col = 0;
        for (auto &entry : columnWidths) {
            _columnNames[col] = string(entry.first);
            _columnWidths[col++] = max(entry.second, Tool::terminalStringWidth(entry.first));
        }

        writeColumnTitles();
        for (Array::iterator row(_rows); row; ++row) {
            Dict rowDict = row->asArray()[0].asDict();
            col = 0;
            for (auto &colName : _columnNames)
                writeValue(col++, rowDict[colName]);
            cout << "\n";
        }
    }

    void writeColumnTitles() {
        cout << tool.ansiBold();
        for (size_t col = 0; col < _numColumns; ++col)
            writeCol(col, _columnNames[col], 1);
        cout << "\n";
        for (size_t col = 0; col < _numColumns; ++col)
            cout << string(_columnWidths[col], '_') << ' ';
        cout << tool.ansiReset() << "\n";
    }

    void writeValue(size_t col, Value val) {
        auto type = val.type();
        if (type == kFLString)
            writeCol(col, val.asString(), 1);
        else if (type != kFLUndefined)
            writeCol(col, val.toJSON(tool._json5, true), (type == kFLNumber ? -1 : 1));
        else
            writeCol(col, nullslice, 0);
    }

    // Subroutine that writes a column:
    void writeCol(size_t col, slice s, int align) {
        string pad(_columnWidths[col] - Tool::terminalStringWidth(s), ' ');
        if (align < 0)
            cout << pad;
        cout << string_view(s);
        if (col < _numColumns - 1) {
            if (align > 0)
                cout << pad;
            cout << ' ';
        }
    }

private:
    vector<unsigned>_columnWidths;
    size_t          _numColumns;
    MutableArray    _rows;
};
