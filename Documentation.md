# `cblite` Tool Documentation

`cblite` is a command-line tool for inspecting and querying [LiteCore][LITECORE] and [Couchbase Lite][CBL] databases (which are directories with a `.cblite2` extension.)

In one-shot usage the first argument is a subcommand name, followed by optional flag arguments and then (usually) a database path. The documentation for each subcommand, below, describes its specific parameters. 

| Command        | Purpose |
|----------------|---------|
| `cblite cat`   | Display the body of one or more documents |
|`cblite check` | Checks the database file for corruption |
|`cblite compact` | Compacts the database file, freeing up disk space |
| `cblite cp`    | Replicate, import or export a database |
| `cblite help`  | Display help text |
| `cblite info`  | Display information about the database |
|`cblite logcat` | Converts binary log files to plain text |
| `cblite ls`    | List the documents in the database |
| `cblite put`   | Create or update a document |
| `cblite query` | Run queries, using the [JSON Query Schema][QUERY] |
|`cblite reindex` | Rebuilds indexes, which may improve performance |
| `cblite revs`  | List the revisions of a document |
| `cblite rm`    | Delete a document |
| `cblite select`| Run queries, using [N1QL][N1QL] syntax |
| `cblite serve` | Start a (rudimentary) REST API listener |

# Interactive Mode

The tool has an interactive mode that you start by running `cblite /path/to/database`, i.e. with no subcommand. It will then prompt you for commands: each command is a command line without the initial `cblite` or the database-path parameter. Enter `quit` or press Ctrl-D to exit. For example:

```
$ cblite travel-sample.cblite2
Opened read-only database travel-sample.cblite2/
(cblite) ls -l --limit 5
Document ID             Rev ID     Flags   Seq     Size
airline_10              1-d70614ae ---       1     0.1K
airline_10123           1-091f80f6 ---       2     0.1K
airline_10226           1-928c43f4 ---       3     0.1K
airline_10642           1-5cb6252c ---       4     0.1K
airline_10748           1-630b0443 ---       5     0.1K
(Stopping after 5 docs)
(cblite) quit
$
```

# Global Flags

These flags go immediately after `cblite`.

| Flag          | Effect  |
|---------------|---------|
| `--color` | Enables ANSI color (or else set the environment variable `CLICOLOR`.) |
| `--create` | Creates a new database if the path does not exist. Opens database in writeable mode. |
| `--encrypted` | Opens encrypted database, prompting for password or key |
| `--help`      | Prints help text, then exits |
| `--version`   | Prints the version of LiteCore, then exits |
| `--writeable` | Opens the database in writeable mode, allowing use of commands like `compact` and `put`. |

Notes on encryption:
* The `--encrypted` flag isn't necessary in interactive mode; the tool will automatically prompt if it finds the database is encrypted. (In non-interactive mode the tool shouldn't block for input unless you meant it to.)
* If your database is encrypted with an AES256 key instead of a password, enter it as 64 hex digits.

# Sub-Commands

(You can run `cblite --help` to get a quick summary, or `cblite help CMD` for help on _CMD_.)

>NOTE: If the first non-flag argument begins with a "`-`", it will be misinterpreted as a flag. To work around this, put an empty flag argument "`--`" before it.

## cat

Displays the JSON body of a document, or of all documents whose IDs match a pattern.

`cblite cat` _[flags]_ _databasepath_ _DOCID_ [_DOCID_ ...]

`cat` _[flags]_ _DOCID_ [_DOCID_ ...]

(DOCID may contain shell-style wildcards `*`, `?`)

| Flag    | Effect  |
|---------|---------|
| `--key KEY` | Display only a single key/value (may be used multiple times) |
| `--rev` | Show the revision ID(s) |
| `--raw` | Raw JSON (not pretty-printed) |
| `--json5` | JSON5 syntax (no quotes around dict keys) |

## check

Performs an integrity check on the database, reporting any signs of corruption.

`cblite check` _databasepath_

`check`

## cp (aka export, import, push, pull)

Copies a database, imports or exports JSON, or replicates.

`cblite cp` _[flags]_ _source_ _destination_

_source_ and _destination_ can be database paths, replication URLs, or JSON file paths. One of them must be a database path ending in `*.cblite2`. The other can be any of the following:

* `*.cblite2` ⟶  Copies local db file, and assigns new UUID to target \*
* `ws://*`  ⟶  Networked replication
* `*.json`    ⟶  Imports/exports JSON file (one document per line)
* `*/`        ⟶  Imports/exports directory of JSON files (one per doc)

\* The `--replicate` flag can be used to force a local-to-local copy to use the replicator. If the command is invoked as `push` or `pull`, this flag is implicitly set.

`cp` _[flags]_ _destination_

In interactive mode, the database path is already known, so it's used as the source, and `cp` takes only a destination argument. You can optionally call the command `push` or `export`. Or if you use the synonyms `pull` or `import` in interactive mode, the parameter you give is treated as the _source_, while the current database is the _destination_.

| Flag    | Effect  |
|---------|---------|
| `--bidi` | Bidirectional (push+pull) replication |
|`-cacert` _file_ | Use X.509 certificate(s) in _file_ (PEM or DER format) to validate the server TLS certificate. Necessary if the server has a self-signed certificate. |
| `--careful` | Abort on any error. |
|`-cert` _file_ | Use X.509 certificate in _file_ (PEM or DER format) for TLS client authentication. Requires `--key`. |
| `--continuous` | Continuous replication (never stops!) |
| `--existing` or `-x` | Fail if _destination_ doesn't already exist.|
| `--jsonid` _property_ | JSON property to use for document ID\*\* |
|`--key` _file_ | Use private key in _file_ for TLS client authentication. Requires `--cert`. |
| `--limit` _n_ | Stop after _n_ documents. (Replicator ignores this) |
| `--replicate` | Forces use of replicator when copying local to local |
| `--user` _name[`:`password]_ | Credentials for remote server. (If password is not given, the tool will prompt you to enter it.) |
| `--verbose` or `-v` | Log progress information. Repeat flag for more verbosity. |

\*\* `--jsonid` works as follows: When _source_ is JSON, this is a property name/path whose value will be used as the document ID. (If omitted, documents are given UUIDs.) When _destination_ is JSON, this is a property name that will be added to the JSON, whose value is the document's ID. (If this flag is omitted, the value defaults to `_id`.)


## help

Displays a list of all commands, or details of a given command.

`cblite help` _[subcommand]_

`help` _[subcommand]_

## info (aka file)

Shows information about the database, such as the number of documents and the latest sequence number. 
With the sub-subcommand `indexes`, it instead lists all the indexes in the database. 
With the sub-subcommand `index` followed by an index name, it instead dumps the entire contents (keys and values) of that index.

`cblite info` _databasepath_ 
`cblite info` _databasepath_ `indexes` 
`cblite info` _databasepath_ `index` _indexname_

`info` 
`info indexes` 
`info index` _indexname_ 

## logcat

Reads Couchbase Lite binary log files, and writes their plain text equivalent to stdout.

Multiple files are merged together with the lines sorted chronologically.

If given a directory path, all `.cbllog` files in that directory are read.

`cblite logcat` _[logfile]_ [_logfile_ ...]
`cblite logcat` _[directory]_

`logcat` _[logfile]_ [_logfile_ ...]
`logcat` _[directory]_

| Flag    | Effect  |
|---------|---------|
| `--csv` | Output in CSV format, per RFC4180. |
|`--full` | Output starts at the first time when full logs (all levels) are available. This is often useful since the less-active log levels preserve a longer history, so if everything is logged then the start of the output will be nothing but old errors and warnings. |
|`--out` _filepath_ | Writes output to a file instead of stdout. |

## ls

Lists the IDs of documents in the database.

`cblite ls` _[flags]_ _databasepath_ _[PATTERN]_

`ls` _[flags]_ _[PATTERN]_

| Flag    | Effect  |
|---------|---------|
| `-l` | Long format (one doc per line, with metadata) |
| `--offset` _n_ | Skip first _n_ docs |
| `--limit` _n_ | Stop after _n_ docs |
| `--desc` | Descending order |
| `--seq` | Order by sequence, not docID |
| `--del` | Include deleted documents |
| `--conf` | Include _only_ conflicted documents |
| `--body` | Display document bodies |
| `--pretty` | Pretty-print document bodies (implies `--body`) |
| `--json5` | JSON5 syntax, i.e. unquoted dict keys (implies `--body`)|

(PATTERN is an optional pattern for matching docIDs, with shell-style wildcards `*`, `?`)

## put

Creates or updates a document.

`cblite put` _[flags]_ _databasepath_ _DOCID_ "_JSON_"

`put` _[flags]_ _DOCID_ "_JSON_"

| Flag    | Effect  |
|---------|---------|
| `--create` | Only create a document; fails if the document exists. |
| `--update` | Only update an existing document; fails if the document does not exist. |

**The document body JSON must be a single argument**; put quotes around it to ensure that and to avoid misinterpretation of special characters. JSON5 syntax is allowed.

> NOTE: In the interactive mode, this command will fail unless `cblite` was invoked with the `--writeable` or `--create` flag.

## query

[Queries][QUERY] the database, using JSON syntax. (See also **select**, to query with N1QL.)

`cblite query` _[flags]_ _databasepath_ "_query_"

`query` _[flags]_ _query_

| Flag    | Effect  |
|---------|---------|
| `--offset` _n_ | Skip first _n_ rows |
| `--limit` _n_ | Stop after _n_ rows |

The _query_ must follow the [JSON query schema][QUERY]. ([JSON5](http://json5.org) syntax is allowed.) It can be a dictionary {`{ ... }`) containing an entire query specification, or an array (`[ ... ]`) with just a `WHERE` clause. There are examples of each up above.

If you're running `cblite query ...` from a shell, you'll need to quote the JSON.

## reindex

Rebuilds indexes. This could be time consuming on a large database. Usually not needed, but it could improve query performance somewhat, because an index built all at once may have a more efficient structure than one that's been incrementally modified over time. 

This could be worthwhile to run as a final step when preparing a database to be embedded inside an application.

`cblite reindex` _databasepath_

`reindex`

> NOTE: In the interactive mode, this command will fail unless `cblite` was invoked with the `--writeable` or `--create` flag.

## revs

Displays the revision history of a document.

`cblite revs` _databasepath_ _DOCID_

`revs` _DOCID_

## rm

Deletes a document.

`cblite rm` _databasepath_ _DOCID_

`rm` _DOCID_

> NOTE: In the interactive mode, this command will fail unless `cblite` was invoked with the `--writeable` or `--create` flag.

## select

Queries the database using [N1QL][N1QL] syntax.

`cblite select` _[flags]_ _databasepath_ "_query_"

`select` _[flags]_ _query_

| Flag    | Effect  |
|---------|---------|
| `--offset` _n_ | Skip first _n_ rows |
| `--limit` _n_ | Stop after _n_ rows |

_query_ is a N1QL `SELECT` query, minus the keyword `SELECT` since that's already been given as the command name. No `FROM` clause is needed since there's only one key-space (the database).

## serve

Runs a mini HTTP server that responds to the {Sync Gateway, Couchbase Lite 1.x, CouchDB, PouchDB, etc.} REST API.

`cblite serve` _[flags]_ _databasepath_

`serve` _[flags]_

| Flag    | Effect  |
|---------|---------|
| `--port` _n_ | Set TCP port number (default is 59840) |
| `--readonly` | Prevent REST calls from altering the database |
| `--verbose` or `-v` | Log requests. Repeat flag for more verbosity. |

**Note:** Only a subset of the REST API is implemented so far! See [the documentation](https://github.com/couchbase/couchbase-lite-core/wiki/REST-API).


# Example

```
$  cblite file travel-sample.cblite2
Database:   travel-sample.cblite2/
Total size: 34MB
Documents:  31591, last sequence 31591

$  cblite ls -l --limit 10 travel-sample.cblite2
Document ID     Rev ID     Flags   Seq     Size
airline_10      1-d70614ae ---       1     0.1K
airline_10123   1-091f80f6 ---       2     0.1K
airline_10226   1-928c43f4 ---       3     0.1K
airline_10642   1-5cb6252c ---       4     0.1K
airline_10748   1-630b0443 ---       5     0.1K
airline_10765   1-e7999661 ---       6     0.1K
airline_109     1-bd546abb ---       7     0.1K
airline_112     1-ca955c69 ---       8     0.1K
airline_1191    1-28dbba6e ---       9     0.1K
airline_1203    1-045b6947 ---      10     0.1K
(Stopping after 10 docs)

$  cblite travel-sample.cblite2
(cblite) query --limit 10 ["=", [".type"], "airline"]
["_id": "airline_10"]
["_id": "airline_10123"]
["_id": "airline_10226"]
["_id": "airline_10642"]
["_id": "airline_10748"]
["_id": "airline_10765"]
["_id": "airline_109"]
["_id": "airline_112"]
["_id": "airline_1191"]
["_id": "airline_1203"]
(Limit was 10 rows)
(cblite) query --limit 10 '{WHAT: [[".name"]], WHERE:  ["=", [".type"], "airline"], ORDER_BY: [[".name"]]}'
["40-Mile Air"]
["AD Aviation"]
["ATA Airlines"]
["Access Air"]
["Aigle Azur"]
["Air Austral"]
["Air Caledonie International"]
["Air CaraÃ¯bes"]
["Air Cargo Carriers"]
["Air Cudlua"]
(Limit was 10 rows)
(cblite) ^D
$
```


[LITECORE]: https://github.com/couchbase/couchbase-lite-core
[CBL]: https://www.couchbase.com/products/lite
[N1QL]: https://docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/index.html
[QUERY]: https://github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
[REST_API]: https://github.com/couchbase/couchbase-lite-core/wiki/REST-API
