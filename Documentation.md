# `cblite` Tool Documentation

`cblite` is a command-line tool for inspecting and querying [LiteCore][LITECORE] and [Couchbase Lite][CBL] databases (which are directories with a `.cblite2` extension.)

It has the following sub-commands:

| Command        | Purpose |
|----------------|---------|
| `cblite cat`   | Display the body of one or more documents |
| `cblite cp`    | Replicate, import or export a database |
| `cblite file`  | Display information about the database |
| `cblite help`  | Display help text |
| `cblite ls`    | List the documents in the database |
| `cblite put`   | Create or update a document |
| `cblite query` | Run queries, using the [JSON Query Schema][QUERY] |
| `cblite revs`  | List the revisions of a document |
| `cblite rm`    | Delete a document |
| `cblite serve` | Starts a (rudimentary) REST API listener |

# Interactive Mode

The tool has an interactive mode that you start by running `cblite /path/to/database`, i.e. with no subcommand. It will then prompt you for commands: each command is a command line without the initial `cblite` or the database-path parameter. Enter `quit` or press Ctrl-D to exit.

When starting interactive mode, you can put a few flags before the database path:

| Flag    | Effect  |
|---------|---------|
| `--create` | Creates a new database if the path does not exist. Opens database in writeable mode. |
| `--writeable` | Opens the database in writeable mode, allowing use of the `put` and `rm` subcommands. |

> **These flags were added after version 2.1.**

# Global Flags

These flags go immediately after `cblite`. No subcommand should be given.

| Flag          | Effect  |
|---------------|---------|
| `--help`      | Prints help text, then exits |
| `--version`   | Echoes the version of LiteCore, then exits |


# Sub-Commands

(You can run `cblite --help` to get a quick summary, or `cblite help CMD` for help on _CMD_.)

## cat

Displays the JSON body of a document (or of documents whose IDs match a pattern.)

`cblite cat` _[flags]_ _databasepath_ _DOCID_ [_DOCID_ ...]

`cat` _[flags]_ _DOCID_ [_DOCID_ ...]

(DOCID may contain shell-style wildcards `*`, `?`)

| Flag    | Effect  |
|---------|---------|
| `--key KEY` | Display only a single key/value (may be used multiple times) |
| `--rev` | Show the revision ID(s) |
| `--raw` | Raw JSON (not pretty-printed) |
| `--json5` | JSON5 syntax (no quotes around dict keys) |

## cp (aka export, import, push, pull)

Copies a database, imports or exports JSON, or replicates.

`cblite cp` _[flags]_ _source_ _destination_

_source_ and _destination_ can be database paths, replication URLs, or JSON file paths. One of them must be a database path ending in `*.cblite2`. The other can be any of the following:

* `*.cblite2` ⟶  Copies local db file, and assigns new UUID to target \*
* `blip://*`  ⟶  Networked replication
* `*.json`    ⟶  Imports/exports JSON file (one document per line)
* `*/`        ⟶  Imports/exports directory of JSON files (one per doc)

\* The `--replicate` flag can be used to force a local-to-local copy to use the replicator. If the command is invoked as `push` or `pull`, this flag is implicitly set.

`cp` _[flags]_ _destination_

In interactive mode, the database path is already known, so it's used as the source, and `cp` takes only a destination argument. You can optionally call the command `push` or `export`. Or if you use the synonyms `pull` or `import` in interactive mode, the parameter you give is treated as the _source_, while the current database is the _destination_.

| Flag    | Effect  |
|---------|---------|
| `--bidi` | Bidirectional (push+pull) replication |
| `--careful` | Abort on any error. |
| `--continuous` | Continuous replication (never stops!) |
| `--existing` or `-x` | Fail if _destination_ doesn't already exist.|
| `--jsonid` _property_ | JSON property to use for document ID\*\* |
| `--limit` _n_ | Stop after _n_ documents. (Replicator ignores this) |
| `--replicate` | Forces use of replicator when copying local to local |
| `--user` _name[`:`password]_ | Credentials for remote server. (If password is not given, the tool will prompt you to enter it.) |
| `--verbose` or `-v` | Log progress information. Repeat flag for more verbosity. |

\*\* `--jsonid` works as follows: When _source_ is JSON, this is a property name/path whose value will be used as the document ID. (If omitted, documents are given UUIDs.) When _destination_ is JSON, this is a property name that will be added to the JSON, whose value is the document's ID. (If this flag is omitted, the value defaults to `_id`.)


## file

Shows information about the database, such as the number of documents and the latest sequence number.

`cblite file` _databasepath_

`file` _databasepath_

## help

Displays a list of all commands, or details of a given command.

`cblite help` _[subcommand]_

`help` _[subcommand]_

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

> **This command was added after version 2.1.**

`cblite put` _[flags]_ _databasepath_ _DOCID_ "_JSON_"

`put` _[flags]_ _DOCID_ "_JSON_"

| Flag    | Effect  |
|---------|---------|
| `--create` | Only create a document; fails if the document exists. |
| `--update` | Only update an existing document; fails if the document does not exist. |

**The document body JSON must be a single argument**; put quotes around it to ensure that and to avoid misinterpretation of special characters. JSON5 syntax is allowed.

> NOTE: In the interactive mode, this command will fail unless `cblite` was invoked with the `--writeable` or `--create` flag.

## query

[Queries][QUERY] the database.

`cblite query` _[flags]_ _databasepath_ "_query_"

`query` _[flags]_ "_query_"

| Flag    | Effect  |
|---------|---------|
| `--offset` _n_ | Skip first _n_ rows |
| `--limit` _n_ | Stop after _n_ rows |

The _query_ must follow the [JSON query schema][QUERY]. It can be a dictionary {`{ ... }`) containing an entire query specification, or an array (`[ ... ]`) with just a `WHERE` clause. There are examples of each up above.

**The query must be a single argument**; put quotes around it to ensure that and to avoid misinterpretation of special characters. [JSON5](http://json5.org) syntax is allowed. 

## revs

Displays the revision history of a document.

`cblite revs` _databasepath_ _DOCID_

`revs` _DOCID_

## rm

> **This command was added after version 2.1.**

Deletes a document.

`cblite rm` _databasepath_ _DOCID_

`rm` _DOCID_

> NOTE: In the interactive mode, this command will fail unless `cblite` was invoked with the `--writeable` or `--create` flag.

## serve

Runs a mini HTTP server that responds to the Couchbase Lite REST API.

`cblite serve` _[flags]_ _databasepath_

`serve` _[flags]_

| Flag    | Effect  |
|---------|---------|
| `--port` _n_ | Set TCP port number (default is 59840) |
| `--readonly` | Prevent REST calls from altering the database |
| `--verbose` or `-v` | Log requests. Repeat flag for more verbosity. |

**Note:** Only a subset of the Couchbase Lite REST API is implemented so far! See [the documentation][REST-API].


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
(cblite) query --limit 10 '["=", [".type"], "airline"]'
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
[QUERY]: https://github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
[REST_API]: https://github.com/couchbase/couchbase-lite-core/wiki/REST-API
