# `cblite` Tool Documentation

`cblite` is a command-line tool for inspecting and querying [LiteCore][LITECORE] and [Couchbase Lite][CBL] databases (which are directories with a `.cblite2` extension.)

In one-shot usage the first argument is a subcommand name, followed by optional flag arguments and then (usually) a database path. For example:

```sh
$ cblite info travel-sample.cblite2
```

Or you can run the tool in an interactive mode, like a cute little special-purpose shell, by omitting the subcommand name:

```sh
$ cblite travel-sample.cblite2
```

# Sub-Command List

Later sections document each subcommand and its specific parameters. 

| Subcommand     | Purpose                                                        |
|----------------|----------------------------------------------------------------|
| `cat`, `get`   | Display the body of one or more documents                      |
| `cd`           | Set the current collection                                     |
| `check`        | Check the database file for corruption                         |
| `compact`      | Compact the database file, freeing up disk space ✍️            |
| `cp`           | Replicate, import or export a database                         |
| `decrypt`      | Remove encryption from a database 👔 ✍️                        |
| `encrypt`      | Encrypt or rekey a database 👔 ✍️                              |
| `edit`         | Update or create a document as JSON in a text editor ✍️        |
| `export`       | Copy documents to a JSON file                                  |
| `help`         | Display help text                                              |
| `import`       | Import documents from a JSON file ✍️                           |
| `info`, `file` | Display information about the database                         |
| `ls`           | List the documents in the current collection                   |
| `lscoll`       | List the collections in the database                           |
| `mkcoll`       | Create a collection ✍️                                         |
| `mkindex`      | Create an index ✍️                                             |
| `mv`           | Move documents to a different collection ✍️                    |
| `openremote`   | Pull a remote DB to a temp file and open it in interactive mode |
| `push`         | Replicate changes to a remote database                         |
| `pull`         | Replicate changes from a remote database ✍️                    |
| `put`          | Create or update a document ✍️                                 |
| `query`        | Run queries, using the [JSON Query Schema][QUERY]              |
| `reindex`      | Rebuild indexes, which may improve performance ✍️              |
| `revs`         | List the revisions of a document                               |
| `rm`           | Delete documents ✍️                                            |
| `rmindex`      | Remove an index ✍️                                             |
| `select`       | Run queries, using [SQL++][N1QL] syntax                        |

> 👔 denotes features only available in Enterprise Edition
>
> ✍️ denotes commands that modify the database, requiring use of the `--writeable` global flag

# Interactive Mode

The tool has an interactive mode that you start by running `cblite /path/to/database`, i.e. with no subcommand. It will then prompt you for commands: each command is a command line without the initial `cblite` or the database-path parameter. Enter `quit` or press Ctrl-D to exit. For example:

```sh
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

These flags go immediately after `cblite`, *before* a subcommand or database name.

| Flag                | Effect                                                                                                                    |
|---------------------|---------------------------------------------------------------------------------------------------------------------------|
| `--color`           | Enable ANSI colors and text styles in the output (to turn on color by default, set the environment variable `$CLICOLOR`.) |
| `--create`          | Create a new database if the path does not exist, and open it in writeable mode. ✍️                                       |
| `--encrypted`       | Open encrypted database, prompting for password or key 👔                                                                 |
| `--help`            | Print help text, then exit                                                                                                |
| `--upgrade`         | Allow DB format to be upgraded to latest version.<br /> ⚠️ May make database unopenable by older versions of CBL.         |
| `--upgrade=vv`      | Upgrade database to use version vectors. <br />⚠️ Experimental! Irreversible!                                             |
| `--version` or `-v` | Print the version of the tool and of LiteCore, then exit                                                                  |
| `--writeable`       | Open the database in writeable mode, allowing use of commands like `compact` and `put` (marked ✍️ in this documentation.) |

Notes on encryption:
* The `--encrypted` flag isn't necessary in interactive mode; the tool will automatically prompt if it finds the database is encrypted. (In non-interactive mode the tool shouldn't block for input unless you meant it to.)
* If your database is encrypted with an AES256 key instead of a password, enter it as 64 hex digits.

# Sub-Command Details

(You can run `cblite --help` to get a quick summary, or `cblite help CMD` for help on _CMD_.)

>NOTE: If a subcommand's first non-flag argument begins with a "`-`", it will be misinterpreted as a flag. You may run into this with document IDs. The workaround is to add an empty flag argument "`--`" to denote the end of the flags.

## cat (*aka* get)

Displays the JSON body of a document, or of all documents whose IDs match a pattern.

`cblite cat` _[flags]_ _databasepath_ _DOCID_ [_DOCID_ ...]

`cat` _[flags]_ _DOCID_ [_DOCID_ ...]

(DOCID may contain shell-style "glob" wildcards `*`, `?`)

| Flag        | Effect                                                       |
|-------------|--------------------------------------------------------------|
| `--key KEY` | Display only a single key/value (may be used multiple times) |
| `--rev`     | Show the revision ID(s)                                      |
| `--raw`     | Raw JSON (not pretty-printed)                                |
| `--json5`   | [JSON5][] syntax (no quotes around dict keys)                |

## cd

Sets the current collection. Initially the `_default` collection is current. Document-oriented commands operate only on the current collection.

If no argument is given, returns to the default collection.

To name a collection not in the default scope, prepend the scope name and a `.` character.

## check

Performs an integrity check on the database, reporting any signs of corruption.

`cblite check` _databasepath_

`check`

## compact ✍️

Compacts the database file, removing internal free space and garbage-collecting obsolete blobs.

| Flag             | Effect                                          |
|------------------|-------------------------------------------------|
| `--prune` *N*    | Also prunes revision trees to maximum depth *N*. |
| `--purgeDeleted` | Also purges _all_ deleted documents.            |

## cp (*aka* export, import, push, pull)

Copies a database, imports or exports JSON, or replicates. 

The `export`, `import`, `pull` and `push` commands are all specific cases of this, with simplified parameters. It's often simpler to use one of those instead.

`cblite cp` _[flags]_ _source_ _destination_

_source_ and _destination_ can be database paths, replication URLs, or JSON file paths. One of them must be a database path ending in `.cblite2`. The other can be any of the following:

* `*.cblite2` ⟶  Copies local db file, and assigns new UUID to target \*
* `ws://*` or `wss://*`  ⟶  Networked replication
* `*.json`    ⟶  Imports/exports JSON file (one document per line)
* `*/`        ⟶  Imports/exports directory of JSON files (one per doc)

\* The `--replicate` flag can be used to force a local-to-local copy to use the replicator. If the command is invoked as `push` or `pull`, this flag is implicitly set. 👔

`cp` _[flags]_ _destination_

In interactive mode, the database path is already known, so it's used as the source, and `cp` takes only a destination argument. You can optionally call the command `push` or `export`. Or if you use the synonyms `pull` or `import` in interactive mode, the parameter you give is treated as the _source_, while the current database is the _destination_.

| Flag                         | Effect  |
|------------------------------|---------|
| `--bidi`                     | Bidirectional (push+pull) replication |
| `-cacert` _file_             | Use X.509 CA certificate(s) in _file_ (PEM or DER format) to validate the server TLS certificate. Necessary if the server has a self-signed certificate. |
| `--careful`                  | Abort on any error. |
| `-cert` _file_               | Use X.509 certificate in _file_ (PEM or DER format) for TLS _client_ authentication. Requires `--key`. 👔 |
| `--collection` *name*        | Adds a collection to the list of collections to be replicated. |
| `--continuous`               | Continuous replication (never stops!) |
| `--existing` or `-x`         | Fail if _destination_ doesn't already exist.|
| `--idprefix` *str*           | When `--jsonid` is in use, adds *str* as a prefix to the document ID. |
| `--jsonid` _property_        | JSON property to use for document ID.\*\* |
| `--key` _file_               | Use private key in _file_ for TLS client authentication. Requires `--cert`. 👔 |
| `--limit` _n_                | Stop after _n_ documents. (Replicator ignores this.) |
| `--replicate`                | Forces use of replicator when copying local-to-local. 👔 |
| `--rootcerts` _file_         | Add trusted root certificates from a PEM or DER file. |
| `--token` *tok*              | Session authentication token for remote database. |
| `--user` _name[`:`password]_ | HTTP Basic auth credentials for remote server. (If password is not given, the tool will prompt you to enter it.) |
| `--verbose` or `-v`          | Log progress information. Repeat flag for more verbosity. |

\*\* `--jsonid` works as follows: When _source_ is JSON, this is a property name/path whose value will be used as the document ID. (If omitted, documents are given UUIDs.) When _destination_ is JSON, this is a property name that will be added to the JSON, whose value is the document's ID. (If this flag is omitted, the value defaults to `_id`.)

## decrypt ✍️ 👔

Removes encryption from the database file.

`cblite decrypt` *databasepath*

`decrypt` 

## edit ✍️

Opens a text editor with the document's JSON body; if you save changes and close the editor, they will be saved to the document.

If the document doesn't exist, the editor opens with an empty JSON object; saving will create the document.

| Flag              | Effect                                                       |
| ----------------- | ------------------------------------------------------------ |
| `--with` *editor* | Specifies name/path of editor program; default is environment variable `$EDITOR`. |
| `--raw`           | Don't pretty-print the JSON.                                 |
| `--json5`         | Uses the more human-friendly [JSON5][JSON5] syntax to write & parse the body. |

## encrypt ✍️ 👔

Encrypts the database file. If the database is already encrypted, changes the encryption key.

`cblite encrypt` *databasepath* *password*

`encrypt` *password*

| Flag    | Effect                                                       |
| ------- | ------------------------------------------------------------ |
| `--raw` | Argument is a raw AES256 key, i.e. 64 hex digits, instead of a password. |

## help

Displays a list of all commands, or details of a given command.

`cblite help` _[subcommand]_

`help` _[subcommand]_

## info (*aka* file)

Shows information about the database, such as the number of documents and the latest sequence number. 
With the sub-subcommand `indexes`, it instead lists all the indexes in the database. 
With the sub-subcommand `index` followed by an index name, it instead dumps the entire contents (keys and values) of that index.

`cblite info` _databasepath_ 
`cblite info` _databasepath_ `indexes` 
`cblite info` _databasepath_ `index` _indexname_

`info` 
`info indexes` 
`info index` _indexname_ 

| Flag              | Effect            |
| ----------------- |-------------------|
| `--verbose`, `-v` | Adds more detail. |

## ls

Lists the IDs of documents in the current collection.

`cblite ls` _[flags]_ _databasepath_ _[PATTERN]_

`ls` _[flags]_ _[PATTERN]_

| Flag           | Effect                                                                   |
|----------------|--------------------------------------------------------------------------|
| `-l`           | Long format (one doc per line, with metadata)                            |
| `--offset` _n_ | Skip first _n_ docs                                                      |
| `--limit` _n_  | Stop after _n_ docs                                                      |
| `--desc`       | Descending order                                                         |
| `--seq`        | Order by sequence, not docID                                             |
| `--del`        | Include deleted documents                                                |
| `--conf`       | Include _only_ conflicted documents                                      |
| `--body`       | Display document bodies                                                  |
| `--raw`        | Show version vectors in raw form, and don't pretty-print document bodies |
| `--json5`      | [JSON5][JSON5] syntax, i.e. unquoted dict keys (implies `--body`)        |
| `-c`           | List collections, not documents (same as `lscoll` command)               |

(PATTERN is an optional pattern for matching docIDs, with shell-style wildcards `*`, `?`)

## lscoll

Lists all collections.

## mkcoll ✍️

Creates a collection.

`cblite mkcoll` *databasepath* *NAME*

`mkcoll`  _NAME_

## mkindex ✍️

Creates an index.

`cblite mkindex` *[flags]* *databasepath* *NAME* *EXPRESSION*

`mkindex`  *[flags]* _NAME_ *EXPRESSION*

*EXPRESSION* is the expression to be indexed, often a document property. In interactive mode, it doesn't need to be quoted: everything after the *NAME* parameter is read as-is.

| Flag       | Effect                                                            |
| ---------- |-------------------------------------------------------------------|
| `--json`   | Use JSON query syntax for *EXPRESSION* instead of  [SQL++][N1QL]. |
| `--fts`    | Create a Full-Text-Search index                                   |
| `--vector` | Create a vector index 👔                                          |

#### FTS index flags:

| Flag                 | Effect                                                       |
| -------------------- | ------------------------------------------------------------ |
| `--language` LANG    | (Human) language, by name or ISO-369 code: da/danish, nl/dutch, en/english, fi/finnish, fr/french, de/german, hu/hungarian, it/italian, no/norwegian, pt/portuguese, ro/romanian, ru/russian, es/spanish, sv/swedish, tr/turkish |
| `--ignoreDiacritics` | Ignore diacritical (accent) marks                            |
| `--noStemming`       | Don't strip word variations like plurals                     |

#### Vector index flags: 👔

| Flag             | Effect                                         |
| ---------------- | ---------------------------------------------- |
| `--dim` N        | Number of dimensions [required]                |
| `--metric` M     | Distance metric: `euclidean` or `cosine`       |
| `--centroids` N  | Flat clustering with N centroids               |
| `--encoding` ENC | Vector encoding: `none`, `SQ8`, `PQ32x8`, etc. |

> NOTE: Vector indexes require the `CouchbaseLiteVectorSearch` extension. The environment variable `$CBLITE_EXTENSION_PATH` must be set to its parent directory.

## mv ✍️

Moves documents from one collection to another.

`mv` *doc* *collection*

*doc* may contain the wildcard characters `*` and `?`; if so, all docs matching the pattern will be moved.

*doc* is a document ID in the current collection. To choose a document in another collection, prefix it with the collection name and a `/`. 

## open

Starts interactive mode; this is the same as not giving a subcommand.

## openremote

Creates a new temporary database, pulls a remote database into it, and opens it in interactive mode. The temporary database is deleted on exit.

`cblite openremote` *[flags]* *URL*

Takes the same flags as `cp`.

## put ✍️

Creates or updates a document.

`cblite put` _[flags]_ _databasepath_ _DOCID_ "_JSON_"

`put` _[flags]_ _DOCID_ _JSON_

| Flag    | Effect  |
|---------|---------|
| `--create` | Only create a document; fails if the document exists. |
| `--update` | Only update an existing document; fails if the document does not exist. |

**The document body JSON must be a single argument**; put quotes around it to ensure that and to avoid misinterpretation of special characters. [JSON5][JSON5] syntax is allowed.

## query

[Queries][QUERY] the database, using JSON or [SQL++][N1QL] syntax.

`cblite query` _[flags]_ _databasepath_ "_query_"

`query` _[flags]_ _query_

- If the query starts with `{` it's parsed as a full JSON query, with keys `WHAT`, `WHERE`, etc.
- If the query starts with `[` it's interpreted as the `WHERE` component of a JSON query, and all columns will be selected.
- Otherwise it's parsed as SQL++. (Note: The `select` command is slightlier more convenient for this.)

| Flag    | Effect  |
|---------|---------|
| `--offset` _n_ | Skip first _n_ rows |
| `--limit` _n_ | Stop after _n_ rows |
| `--explain` | Show an explanation of the query instead of running it |
| `--raw` | Outputs JSON instead of a human-readable table |

If you're running `cblite query ...` from a shell, you'll need to quote the query to make it a single argument and stop the shell from interpreting special characters.

## reindex ✍️

Rebuilds indexes. This could be time consuming on a large database. Usually not needed, but it could improve query performance somewhat, because an index built all at once may have a more efficient structure than one that's been incrementally modified over time. 

This could be worthwhile to run as a final step when preparing a database to be embedded inside an application.

`cblite reindex` _databasepath_

`reindex`

## revs

Displays the revision history of a document.

`cblite revs` _databasepath_ _DOCID_

`revs` _DOCID_

| Flag        | Effect                                                       |
| ----------- | ------------------------------------------------------------ |
| `--remotes` | Shows which revisions are marked as being current on remote (replicated) databases |
| `--raw`     | Don't abbreviate version vectors                             |

Revision flags are denoted by dashes or the letters:

- D: Deleted
- X: Branch is closed
- C: Conflicting branch
- A: Revision has attachments/blobs
- K: Revision keeps its body even if not current
- L: Leaf revision

## rm ✍️

Deletes a document.

`cblite rm` _databasepath_ _DOCID_

`rm` _DOCID_

> NOTE: In the interactive mode, this command will fail unless `cblite` was invoked with the `--writeable` or `--create` flag.

## rmindex ✍️

Deletes an index.

`cblite rmindex` _databasepath_ _NAME_

`rmindex` _NAME_

## select

A shortcut for `query` that always uses  [SQL++][N1QL] syntax and includes the `SELECT` as the start of the query string.

`cblite select` _[flags]_ _databasepath_ "_query_"

`select` _[flags]_ _query_



[LITECORE]: https://github.com/couchbase/couchbase-lite-core
[CBL]: https://www.couchbase.com/products/lite
[N1QL]: https://docs.couchbase.com/server/current/n1ql/n1ql-language-reference/index.html
[QUERY]: https://github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
[REST_API]: https://github.com/couchbase/couchbase-lite-core/wiki/REST-API
[JSON5]: https://json5.org
