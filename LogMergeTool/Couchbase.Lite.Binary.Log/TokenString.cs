// 
// TokenString.cs
// 
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 

namespace Couchbase.Lite.Binary.Log
{
    /// <summary>
    /// A token string is a binary log object which is identified by a
    /// numeric ID.  The first time it is present in a binary log it will
    /// contain the value data but subsequently it will only contain the
    /// ID to save space.  This class will hold both.
    /// </summary>
    public sealed class TokenString
    {
        #region Properties

        /// <summary>
        /// Gets the ID of the token string
        /// </summary>
        public uint Id { get; set; }

        /// <summary>
        /// Gets the value of the token string
        /// </summary>
        public string Value { get; }

        #endregion

        #region Constructors

        public TokenString(uint id, string value)
        {
            Id = id;
            Value = value;
        }

        #endregion

        #region Public Methods

        /// <summary>
        /// Implicit conversion to string
        /// </summary>
        /// <param name="str">The token string to convert</param>
        public static implicit operator string(TokenString str) => str.Value;

        #endregion

        #region Overrides

        /// <inheritdoc />
        public override bool Equals(object obj)
        {
            if (!(obj is TokenString other)) {
                return false;
            }

            return other.Id == Id;
        }

        /// <inheritdoc />
        public override int GetHashCode() => (int) Id;

        /// <inheritdoc />
        public override string ToString() => $"{Id}|{Value}";

        #endregion
    }
}