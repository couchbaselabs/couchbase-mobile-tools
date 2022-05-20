//
// RemoteQueryResultWriter.hh
//
// Copyright © 2022 Couchbase. All rights reserved.
//

#pragma once
#include "LiteCoreTool.hh"
#include "fleece/Mutable.hh"
#include <iomanip>
#include <map>
#include <vector>

using namespace std;
using namespace fleece;


/** Abstract class that writes the result rows of a query.
    Used by RemoteQueryCommand. */
class RemoteQueryResultWriter {
public:
    /// Constructor. Given the parent tool instance and an array of column name strings.
    RemoteQueryResultWriter(LiteCoreTool &tool)
    :_tool(tool)
    { }

    virtual ~RemoteQueryResultWriter() = default;

    void setUnwrap(bool unwrap)             {_unwrap = unwrap;}
    void setMaxWidth(unsigned w)            {_maxWidth = w;}

    /// Adds a result row. Each array item is a column value.
    /// Subclasses should increment `_rowNumber`.
    virtual void addRow(slice jsonObject) = 0;

    /// Called after all the rows have been added.
    virtual void end()                      { }

    /// The current row number (from 1), or the number of rows that have been added.
    unsigned rowNumber() const              {return _rowNumber;}

protected:

    /// Textual representation of a value.
    /// Strings are written unescaped, undefined is nothing, and other types are written as JSON.
    string valueRepresentation(Value val) {
        switch (val.type()) {
            case kFLUndefined: return "";
            case kFLString:    return string(val.asString());
            default:           return string(val.toJSON(_tool._json5, true));
        }
    }

    LiteCoreTool&       _tool;
    unsigned            _rowNumber = 0;
    bool                _unwrap = false;
    unsigned            _maxWidth = UINT_MAX;
};


/** Writes query results in JSON form as an array of dicts.
    Each row's dict keys are the column names. */
class RemoteQueryResultJSONWriter : public RemoteQueryResultWriter {
public:
    RemoteQueryResultJSONWriter(LiteCoreTool &tool) :RemoteQueryResultWriter(tool) { }

    virtual void addRow(slice jsonObject) override {
        ++_rowNumber;
        if (_rowNumber == 1)
            cout << "[\n  ";
        else
            cout << ",\n  ";
        cout << string_view(jsonObject) << endl;
    }

    void end() override {
        if (_rowNumber >= 1)
            cout << "]\n";
    }
};


/** Writes query results as a monospaced table, with the column names as headers.
    Because it has to compute column widths, no output is written until `end` is called. */
class RemoteQueryResultTableWriter : public RemoteQueryResultWriter {
public:
    RemoteQueryResultTableWriter(LiteCoreTool &tool) :RemoteQueryResultWriter(tool) { }

    void addRow(slice jsonObject) override {
        ++_rowNumber;
        Doc doc = Doc::fromJSON(jsonObject);
        Dict row = doc.asDict();
        if (_unwrap && row.count() == 1) {
            Dict unwrapped = Dict::iterator(row).value().asDict();
            if (unwrapped)
                row = unwrapped;
        }

        for (Dict::iterator i(row); i; ++i) {
            string key(i.keyString());
            string value = valueRepresentation(i.value());
            Column &column = _columns[key];
            if (column.width == 0)
                column.width = Tool::terminalStringWidth(key);
            auto valWidth = Tool::terminalStringWidth(value);
            column.width = max(column.width, valWidth);
            switch (i.value().type()) {
                case kFLNumber: --column.align; break;
                default:        ++column.align; break;
            }
            column.rows.emplace_back(move(value));
        }
    }

    void end() override {
        constrainColumnWidths();
        writeColumnTitles();
        for (size_t row = 0; row < _rowNumber; ++row) {
            size_t i = 0;
            for (auto &[name, col] : _columns) {
                writeCol(i++, col.width, col.rows[row], col.align);
            }
            cout << endl;
        }
    }

    // Trims column widths to make the table fit in `_maxWidth` characters.
    void constrainColumnWidths() {
        while (true) {
            // Measure the table width, and find the widest column:
            unsigned width = 0;
            Column *maxCol = nullptr;
            for (auto &[name, col] : _columns) {
                width += col.width;
                if (!maxCol || col.width > maxCol->width)
                    maxCol = &col;
            }
            if (width <= _maxWidth - _columns.size()) // account for column separators
                break;                  // -> Done.
            if (maxCol->width <= 2)
                break;                  // -> Give up in pathological cases
            // Decrement the widest column's width and retry...
            --maxCol->width;
        }
    }

    // Writes the column titles.
    void writeColumnTitles() {
        cout << _tool.ansiBold() << _tool.ansiUnderline();
        for (auto &[name, col] : _columns)
            writeCol(0, col.width, name, col.align);
        cout << _tool.ansiReset() << endl;
        if (_tool.ansiUnderline().empty()) {
            for (auto &[name, col] : _columns)
                cout << string(col.width, '_') << ' ';
            cout << endl;
        }
    }

    // Writes a string in a column:
    void writeCol(size_t colNo, size_t colWidth, slice str, int align) {
        // Note: Can't use std::setw() because it measures bytes, not UTF-8 characters.
        auto strWidth = Tool::terminalStringWidth(str);
        bool truncated = false;
        if (strWidth > colWidth) {
            auto origSize = str.size;
            auto trim = strWidth - colWidth + 1;
            do {
                str.setSize(origSize - trim);
                strWidth = Tool::terminalStringWidth(str) + 1;
                ++trim;
            } while (strWidth > _maxWidth);
            truncated = true;
        }

        string pad(colWidth - strWidth, ' ');
        if (align < 0)
            cout << pad;
        cout << string_view(str);
        if (truncated)
            cout << "…";

        if (colNo < _columns.size() - 1) {
            if (align >= 0)
                cout << pad;
            cout << ' ';
        }
    }

private:
    struct Column {
        unsigned        width = 0;  // Terminal character width of the column
        int             align = 0;  // Alignment; negative is right-aligned, else left.
        vector<string>  rows;       // The string to display in each row.
    };

    map<string,Column>  _columns;   // The columns (sorted by name of course)
};
