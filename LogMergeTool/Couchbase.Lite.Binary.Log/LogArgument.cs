using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

using static BinaryEncoding.Binary;

namespace Couchbase.Lite.Binary.Log
{
    /// <summary>
    /// An interface representing an argument in a log line string (i.e.
    /// an argument that will be provided to a printf style string)
    /// </summary>
    public interface ILogArgument
    {
        #region Public Methods

        ValueTask WriteAsync(Stream s);

        #endregion
    }

    // %x %X %u
    internal sealed class VarintLogArgument : ILogArgument
    {
        #region Variables

        private readonly ulong _value;

        #endregion

        #region Constructors

        public VarintLogArgument(ulong value) => _value = value;

        #endregion

        #region ILogArgument

        public async ValueTask WriteAsync(Stream s) => await Varint.WriteAsync(s, _value).ConfigureAwait(false);

        #endregion
    }

    // %c %d %i
    internal sealed class SignedVarintLogArgument : ILogArgument
    {
        #region Variables

        private readonly byte _sign;
        private readonly ulong _value;

        #endregion

        #region Constructors

        public SignedVarintLogArgument(ulong value, bool negative)
        {
            _sign = Convert.ToByte(negative);
            _value = value;
        }

        #endregion

        #region ILogArgument

        public async ValueTask WriteAsync(Stream s)
        {
            s.WriteByte(_sign);
            await Varint.WriteAsync(s, _value).ConfigureAwait(false);
        }

        #endregion
    }

    // %p
    internal sealed class LittleEndianLogArgument : ILogArgument
    {
        #region Variables

        private readonly ulong _value;

        #endregion

        #region Properties

        public int Size { get; }

        #endregion

        #region Constructors

        public LittleEndianLogArgument(ulong value, int size)
        {
            Debug.Assert(size == 4 || size == 8, "Invalid little endian size");
            if (size == 4) {
                Debug.Assert(value <= UInt32.MaxValue, "4-byte value overflow");
            }

            _value = value;
            Size = size;
        }

        #endregion

        #region ILogArgument

        public async ValueTask WriteAsync(Stream s)
        {
            if (Size == 4) {
                await LittleEndian.WriteAsync(s, (uint) _value).ConfigureAwait(false);
            } else {
                await LittleEndian.WriteAsync(s, _value).ConfigureAwait(false);
            }
        }

        #endregion
    }

    // %-s %-@
    internal sealed class TokenStringLogArgument : ILogArgument
    {
        #region Constants

        private static readonly Dictionary<Guid, HashSet<uint>> AlreadyWritten = new Dictionary<Guid, HashSet<uint>>();

        #endregion

        #region Variables

        private Guid _group = Guid.Empty;

        #endregion

        #region Properties

        public TokenString Value { get; set; }

        #endregion

        #region Constructors

        public TokenStringLogArgument(TokenString value)
        {
            Value = value;
        }

        #endregion

        #region Public Methods

        public static void ClearGroup(Guid group)
        {
            AlreadyWritten.Remove(group);
        }

        public void AddToGroup(Guid group)
        {
            if (!AlreadyWritten.ContainsKey(group)) {
                AlreadyWritten[group] = new HashSet<uint>();
            }

            _group = group;
        }

        #endregion

        #region ILogArgument

        public async ValueTask WriteAsync(Stream s)
        {
            Debug.Assert(_group != Guid.Empty);
            await Varint.WriteAsync(s, Value.Id).ConfigureAwait(false);
            var written = AlreadyWritten[_group];
            if (!written.Contains(Value.Id)) {
                written.Add(Value.Id);
                await s.WriteAsync(Encoding.UTF8.GetBytes(Value.Value)).ConfigureAwait(false);
                s.WriteByte(0);
            }
        }

        #endregion
    }

    // %e %E %f %F %g %G %a %A
    internal sealed class DoubleLogArgument : ILogArgument
    {
        #region Variables

        private readonly double _value;

        #endregion

        #region Constructors

        public DoubleLogArgument(double value)
        {
            _value = value;
        }

        #endregion

        #region ILogArgument

        public async ValueTask WriteAsync(Stream s)
        {
            var int64 = BitConverter.DoubleToInt64Bits(_value);
            await LittleEndian.WriteAsync(s, int64);
        }

        #endregion
    }

    // %s
    internal sealed class ByteSliceLogArgument : ILogArgument
    {
        #region Variables

        private readonly List<byte> _value = new List<byte>();

        #endregion

        #region Public Methods

        public unsafe void Append(byte* buf, int len)
        {
            byte* current = buf;
            while (current - buf < len) {
                _value.Add(*current);
                current++;
            }
        }

        #endregion

        #region ILogArgument

        public async ValueTask WriteAsync(Stream s)
        {
            await Varint.WriteAsync(s, (ulong) _value.Count);
            await s.WriteAsync(_value.ToArray()).ConfigureAwait(false);
        }

        #endregion
    }
}