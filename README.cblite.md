# The `cblite` Tool

`cblite` is a command-line tool for inspecting and querying [LiteCore][LITECORE] and [Couchbase Lite][CBL] databases (which are directories with a `.cblite2` extension.)

For build instructions, see [BUILDING.md](BUILDING.md). For convenience, binaries are available in the [Releases](https://github.com/couchbaselabs/couchbase-mobile-tools/releases) tab.

> NOTE: The _source code_ for the tools is Apache-licensed, as specified in [LICENSE](https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/LICENSE). However, the pre-built [cblite](https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/README.cblite.md) binaries are linked with the Enterprise Edition of Couchbase Lite, so the usage _of those pre-built binaries_ will be guided by the terms and conditions specified in Couchbase's [Enterprise License](https://www.couchbase.com/ESLA01162020).

Official support of the `cblite` tool is not yet available. This applies also to the pre-built binaries that are linked with the Enterprise Edition of Couchbase.

## Changes

(For details on flags and subcommands, use the `help` command.)

#### June 2020:

* The Enterprise-Edition build can now open encrypted databases. Interactive mode will automatically prompt for a password; in one-shot mode you must add the `--encrypted` flag before the database name to get the prompt. A raw AES256 key can be entered at the password prompt as a 64-digit hex string.
* Added a `compact` subcommand that compacts the database. It can also prune old revisions and purge deleted documents.
* Added a `logcat` subcommand that displays CBL binary log files.
* Improvements to the `file` subcommand:
  * Renamed to the more memorable `info`. But `file` still works.
  * Now displays more information, like the total on-disk size of document bodies and metadata.
  * `--verbose` flag makes it show even _more_ information.
  * New sub-subcommand `info indexes` lists the database indexes.
  * New sub-subcommand `info index NAME` gives information about the index named `NAME`.
* Improvements to the `cp` (AKA `push`, `pull`) subcommand:
  * Now supports replication over TLS.
  * `--cacert` flag gives a custom X.509 certificate for the replicator to use when validating the server certificate.
  * `--cert` flag gives a certificate for the replicator to use for _client_ authentication to the server.
* The `revs` subcommand now shows closed conflict branches.
* Terminal output uses ANSI colors by default if the semi-standard `CLICOLOR` environment variable is set.

## Features

| Command        | Purpose |
|----------------|---------|
| `cblite cat`   | Display the body of one or more documents |
|`cblite compact` | Compact the database or prune old revisions |
| `cblite cp`    | Replicate a database, or import/export JSON |
| `cblite info`  | Display information about the database |
| `cblite help`  | Display help text |
|`cblite logcat` | Convert binary log files to text |
| `cblite ls`    | List the documents in the database |
| `cblite put`   | Create or update a document |
| `cblite query` | Run queries, using the [JSON Query Schema][QUERY] |
| `cblite revs`  | List the revisions of a document |
| `cblite rm`    | Delete a document |
| `cblite select`| Run queries, using [N1QL][N1QL] syntax |
| `cblite serve` | Starts a (rudimentary) REST API listener |

See the [full documentation](Documentation.md)...

## Example

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
(cblite) query --limit 10 {WHAT: [[".name"]], WHERE:  ["=", [".type"], "airline"], ORDER_BY: [[".name"]]}
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
[N1QL]: https://docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/index.html
