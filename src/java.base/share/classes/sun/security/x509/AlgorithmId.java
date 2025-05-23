/*
 * Copyright (c) 1996, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.security.x509;

import java.io.*;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.security.*;

import sun.security.util.*;


/**
 * This class identifies algorithms, such as cryptographic transforms, each
 * of which may be associated with parameters.  Instances of this base class
 * are used when this runtime environment has no special knowledge of the
 * algorithm type, and may also be used in other cases.  Equivalence is
 * defined according to OID and (where relevant) parameters.
 *
 * <P>Subclasses may be used, for example when the algorithm ID has
 * associated parameters which some code (e.g. code using public keys) needs
 * to have parsed.  Two examples of such algorithms are Diffie-Hellman key
 * exchange, and the Digital Signature Standard Algorithm (DSS/DSA).
 *
 * <P>The OID constants defined in this class correspond to some widely
 * used algorithms, for which conventional string names have been defined.
 * This class is not a general repository for OIDs, or for such string names.
 * Note that the mappings between algorithm IDs and algorithm names is
 * not one-to-one.
 *
 *
 * @author David Brownell
 * @author Amit Kapoor
 * @author Hemma Prafullchandra
 */
public class AlgorithmId implements Serializable, DerEncoder {

    /** use serialVersionUID from JDK 1.1. for interoperability */
    @java.io.Serial
    private static final long serialVersionUID = 7205873507486557157L;

    /**
     * The object identifier being used for this algorithm.
     */
    private ObjectIdentifier algid;

    // The (parsed) parameters
    @SuppressWarnings("serial") // Not statically typed as Serializable
    private AlgorithmParameters algParams;

    /**
     * Parameters for this algorithm.  These are stored in unparsed
     * DER-encoded form; subclasses can be made to automatically parse
     * them so there is fast access to these parameters.
     */
    protected transient byte[] encodedParams;

    /**
     * Constructs an algorithm ID which will be initialized
     * separately, for example by deserialization.
     * @deprecated use one of the other constructors.
     */
    @Deprecated
    public AlgorithmId() { }

    /**
     * Constructs a parameterless algorithm ID.
     *
     * @param oid the identifier for the algorithm
     */
    public AlgorithmId(ObjectIdentifier oid) {
        algid = oid;
    }

    /**
     * Constructs an algorithm ID with algorithm parameters.
     *
     * @param oid the identifier for the algorithm.
     * @param algparams the associated algorithm parameters, can be null.
     * @exception IllegalStateException if algparams is not initialized
     *                                  or cannot be encoded
     */
    public AlgorithmId(ObjectIdentifier oid, AlgorithmParameters algparams) {
        algid = oid;
        this.algParams = algparams;
        if (algParams != null) {
            try {
                encodedParams = algParams.getEncoded();
            } catch (IOException ioe) {
                throw new IllegalStateException(
                        "AlgorithmParameters not initialized or cannot be decoded",
                        ioe);
            }
        }
    }

    /**
     * Constructs an algorithm ID with algorithm parameters as a DerValue.
     *
     * @param oid the identifier for the algorithm.
     * @param params the associated algorithm parameters, can be null.
     */
    @SuppressWarnings("this-escape")
    public AlgorithmId(ObjectIdentifier oid, DerValue params)
            throws IOException {
        this.algid = oid;
        if (params != null) {
            encodedParams = params.toByteArray();
            decodeParams();
        }
    }

    protected void decodeParams() throws IOException {
        String algidName = getName();
        try {
            algParams = AlgorithmParameters.getInstance(algidName);
        } catch (NoSuchAlgorithmException e) {
            /*
             * This algorithm parameter type is not supported, so we cannot
             * parse the parameters.
             */
            algParams = null;
            return;
        }

        // Decode (parse) the parameters
        algParams.init(encodedParams.clone());
    }

    /**
     * DER encode this object onto an output stream.
     * Implements the <code>DerEncoder</code> interface.
     *
     * @param out the output stream on which to write the DER encoding.
     */
    @Override
    public void encode(DerOutputStream out) {
        DerOutputStream bytes = new DerOutputStream();

        bytes.putOID(algid);

        if (encodedParams == null) {
            // MessageDigest algorithms usually have a NULL parameters even
            // if most RFCs suggested absent.
            // RSA key and signature algorithms requires the NULL parameters
            // to be present, see A.1 and A.2.4 of RFC 8017.
            if (algid.equals(RSAEncryption_oid)
                    || algid.equals(MD2_oid)
                    || algid.equals(MD5_oid)
                    || algid.equals(SHA_oid)
                    || algid.equals(SHA224_oid)
                    || algid.equals(SHA256_oid)
                    || algid.equals(SHA384_oid)
                    || algid.equals(SHA512_oid)
                    || algid.equals(SHA512_224_oid)
                    || algid.equals(SHA512_256_oid)
                    || algid.equals(SHA3_224_oid)
                    || algid.equals(SHA3_256_oid)
                    || algid.equals(SHA3_384_oid)
                    || algid.equals(SHA3_512_oid)
                    || algid.equals(SHA1withRSA_oid)
                    || algid.equals(SHA224withRSA_oid)
                    || algid.equals(SHA256withRSA_oid)
                    || algid.equals(SHA384withRSA_oid)
                    || algid.equals(SHA512withRSA_oid)
                    || algid.equals(SHA512$224withRSA_oid)
                    || algid.equals(SHA512$256withRSA_oid)
                    || algid.equals(MD2withRSA_oid)
                    || algid.equals(MD5withRSA_oid)
                    || algid.equals(SHA3_224withRSA_oid)
                    || algid.equals(SHA3_256withRSA_oid)
                    || algid.equals(SHA3_384withRSA_oid)
                    || algid.equals(SHA3_512withRSA_oid)) {
                bytes.putNull();
            }
        } else {
            bytes.writeBytes(encodedParams);
        }
        out.write(DerValue.tag_Sequence, bytes);
    }


    /**
     * Returns the DER-encoded X.509 AlgorithmId as a byte array.
     */
    public final byte[] encode() {
        DerOutputStream out = new DerOutputStream();
        encode(out);
        return out.toByteArray();
    }

    /**
     * Returns the ISO OID for this algorithm.  This is usually converted
     * to a string and used as part of an algorithm name, for example
     * "OID.1.3.14.3.2.13" style notation.  Use the <code>getName</code>
     * call when you do not need to ensure cross-system portability
     * of algorithm names, or need a user-friendly name.
     */
    public final ObjectIdentifier getOID () {
        return algid;
    }

    /**
     * Returns a name for the algorithm which can be used by getInstance()
     * call of a crypto primitive. The name is usually more intelligible
     * to humans than the algorithm's OID, but which won't necessarily
     * be comprehensible on other systems.  For example, this might
     * return a name such as "MD5withRSA" for a signature algorithm on
     * some systems.  It also returns OID names like "1.2.3.4", when
     * no particular name for the algorithm is known. The OID may also be
     * recognized by getInstance() calls since an OID is usually defined
     * as an alias for an algorithm by the security provider.
     *
     * In some special cases where the OID does not include enough info
     * to return a Java standard algorithm name, an algorithm name
     * that includes info on the params is returned:
     *
     * 1. For ecdsa-with-SHA2 plus hash algorithm (Ex: SHA-256), this method
     * returns the "full" signature algorithm (Ex: SHA256withECDSA) directly.
     *
     * 2. For PBES2, this method returns the "full" cipher name containing the
     * KDF and Enc algorithms (Ex: PBEWithHmacSHA256AndAES_256) directly.
     */
    public String getName() {
        String oidStr = algid.toString();
        // first check the list of support oids
        KnownOIDs o = KnownOIDs.findMatch(oidStr);
        if (o == KnownOIDs.SpecifiedSHA2withECDSA) {
            if (encodedParams != null) {
                try {
                    AlgorithmId digestParams =
                        AlgorithmId.parse(new DerValue(encodedParams));
                    String digestAlg = digestParams.getName();
                    return digestAlg.replace("-", "") + "withECDSA";
                } catch (IOException e) {
                    // ignore
                }
            }
        } else if (o == KnownOIDs.PBES2) {
            if (algParams != null) {
                return algParams.toString();
            } else {
                // when getName() is called in decodeParams(), algParams is
                // null, where AlgorithmParameters.getInstance("PBES2") will
                // be used to initialize it.
            }
        }
        if (o != null) {
            return o.stdName();
        } else {
            String n = aliasOidsTable().get(oidStr);
            return (n != null) ? n : algid.toString();
        }
    }

    public AlgorithmParameters getParameters() {
        return algParams;
    }

    /**
     * Returns the DER encoded parameter, which can then be
     * used to initialize java.security.AlgorithmParameters.
     *
     * Note that this* method should always return a new array as it is called
     * directly by the JDK implementation of X509Certificate.getSigAlgParams()
     * and X509CRL.getSigAlgParams().
     *
     * Note: for ecdsa-with-SHA2 plus hash algorithm (Ex: SHA-256), this method
     * returns null because {@link #getName()} has already returned the "full"
     * signature algorithm (Ex: SHA256withECDSA).
     *
     * @return DER encoded parameters, or null not present.
     */
    public byte[] getEncodedParams() {
        return (encodedParams == null ||
            algid.toString().equals(KnownOIDs.SpecifiedSHA2withECDSA.value()))
                ? null
                : encodedParams.clone();
    }

    /**
     * Returns true iff the argument indicates the same algorithm
     * with the same parameters.
     */
    public boolean equals(AlgorithmId other) {
        return algid.equals(other.algid) &&
            Arrays.equals(encodedParams, other.encodedParams);
    }

    /**
     * Compares this AlgorithmID to another.  If algorithm parameters are
     * available, they are compared.  Otherwise, just the object IDs
     * for the algorithm are compared.
     *
     * @param other preferably an AlgorithmId, else an ObjectIdentifier
     */
    @Override
    public boolean equals(Object other) {
        if (this == other) {
            return true;
        }
        if (other instanceof AlgorithmId) {
            return equals((AlgorithmId) other);
        } else if (other instanceof ObjectIdentifier) {
            return equals((ObjectIdentifier) other);
        } else {
            return false;
        }
    }

    /**
     * Compares two algorithm IDs for equality.  Returns true iff
     * they are the same algorithm, ignoring algorithm parameters.
     */
    public final boolean equals(ObjectIdentifier id) {
        return algid.equals((Object)id);
    }

    /**
     * {@return a hashcode for this AlgorithmId}
     */
    @Override
    public int hashCode() {
        int hashCode = algid.hashCode();
        hashCode = 31 * hashCode + Arrays.hashCode(encodedParams);
        return hashCode;
    }

    /**
     * Provides a human-readable description of the algorithm parameters.
     * This may be redefined by subclasses which parse those parameters.
     */
    protected String paramsToString() {
        if (encodedParams == null) {
            return "";
        } else if (algParams != null) {
            return ", " + algParams.toString();
        } else {
            return ", params unparsed";
        }
    }

    /**
     * Returns a string describing the algorithm and its parameters.
     */
    @Override
    public String toString() {
        return getName() + paramsToString();
    }

    /**
     * Parse (unmarshal) an ID from a DER sequence input value.  This form
     * parsing might be used when expanding a value which has already been
     * partially unmarshaled as a set or sequence member.
     *
     * @exception IOException on error.
     * @param val the input value, which contains the algid and, if
     *          there are any parameters, those parameters.
     * @return an ID for the algorithm.  If the system is configured
     *          appropriately, this may be an instance of a class
     *          with some kind of special support for this algorithm.
     *          In that case, you may "narrow" the type of the ID.
     */
    public static AlgorithmId parse(DerValue val) throws IOException {
        if (val.tag != DerValue.tag_Sequence) {
            throw new IOException("algid parse error, not a sequence");
        }

        /*
         * Get the algorithm ID and any parameters.
         */
        ObjectIdentifier        algid;
        DerValue                params;
        DerInputStream          in = val.toDerInputStream();

        algid = in.getOID();
        if (in.available() == 0) {
            params = null;
        } else {
            params = in.getDerValue();
            if (params.tag == DerValue.tag_Null) {
                if (params.length() != 0) {
                    throw new IOException("invalid NULL");
                }
                params = null;
            }
            if (in.available() != 0) {
                throw new IOException("Invalid AlgorithmIdentifier: extra data");
            }
        }

        return new AlgorithmId(algid, params);
    }

    /**
     * Returns one of the algorithm IDs most commonly associated
     * with this algorithm name.
     *
     * @param algname the name being used
     * @deprecated use the short get form of this method.
     * @exception NoSuchAlgorithmException on error.
     */
    @Deprecated
    public static AlgorithmId getAlgorithmId(String algname)
            throws NoSuchAlgorithmException {
        return get(algname);
    }

    /**
     * Returns one of the algorithm IDs most commonly associated
     * with this algorithm name.
     *
     * @param algname the name being used
     * @exception NoSuchAlgorithmException on error.
     */
    public static AlgorithmId get(String algname)
            throws NoSuchAlgorithmException {
        ObjectIdentifier oid;
        try {
            oid = algOID(algname);
        } catch (IOException ioe) {
            throw new NoSuchAlgorithmException
                ("Invalid ObjectIdentifier " + algname);
        }

        if (oid == null) {
            throw new NoSuchAlgorithmException
                ("unrecognized algorithm name: " + algname);
        }
        return new AlgorithmId(oid);
    }

    /**
     * Returns one of the algorithm IDs most commonly associated
     * with this algorithm parameters.
     *
     * @param algparams the associated algorithm parameters.
     * @exception NoSuchAlgorithmException on error.
     * @exception IllegalStateException if algparams is not initialized
     *                                  or cannot be encoded
     */
    public static AlgorithmId get(AlgorithmParameters algparams)
            throws NoSuchAlgorithmException {
        ObjectIdentifier oid;
        String algname = algparams.getAlgorithm();
        try {
            oid = algOID(algname);
        } catch (IOException ioe) {
            throw new NoSuchAlgorithmException
                ("Invalid ObjectIdentifier " + algname);
        }
        if (oid == null) {
            throw new NoSuchAlgorithmException
                ("unrecognized algorithm name: " + algname);
        }
        return new AlgorithmId(oid, algparams);
    }

    /*
     * Translates from some common algorithm names to the
     * OID with which they're usually associated ... this mapping
     * is the reverse of the one below, except in those cases
     * where synonyms are supported or where a given algorithm
     * is commonly associated with multiple OIDs.
     *
     * XXX This method needs to be enhanced so that we can also pass the
     * scope of the algorithm name to it, e.g., the algorithm name "DSA"
     * may have a different OID when used as a "Signature" algorithm than when
     * used as a "KeyPairGenerator" algorithm.
     */
    private static ObjectIdentifier algOID(String name) throws IOException {
        if (name.startsWith("OID.")) {
            name = name.substring("OID.".length());
        }

        KnownOIDs k = KnownOIDs.findMatch(name);
        if (k != null) {
            return ObjectIdentifier.of(k);
        }

        // unknown algorithm oids
        if (!name.contains(".")) {
            // see if there is a matching oid string alias mapping from
            // 3rd party providers
            name = name.toUpperCase(Locale.ENGLISH);
            String oidStr = aliasOidsTable().get(name);
            if (oidStr != null) {
                return ObjectIdentifier.of(oidStr);
            } return null;
        } else {
            return ObjectIdentifier.of(name);
        }
    }

    // oid string cache indexed by algorithm name and oid strings
    private static volatile Map<String,String> aliasOidsTable;

    // called by sun.security.jca.Providers whenever provider list is changed
    public static void clearAliasOidsTable() {
        aliasOidsTable = null;
    }

    // returns the aliasOidsTable, lazily initializing it on first access.
    private static Map<String,String> aliasOidsTable() {
        // Double-checked locking; safe because aliasOidsTable is volatile
        Map<String,String> tab = aliasOidsTable;
        if (tab == null) {
            synchronized (AlgorithmId.class) {
                if ((tab = aliasOidsTable) == null) {
                    aliasOidsTable = tab = collectOIDAliases();
                }
            }
        }
        return tab;
    }

    private static boolean isKnownProvider(Provider p) {
        String pn = p.getName();
        String mn = p.getClass().getModule().getName();
        if (pn != null && mn != null) {
            return ((mn.equals("java.base") &&
                    (pn.equals("SUN") || pn.equals("SunRsaSign") ||
                    pn.equals("SunJCE") || pn.equals("SunJSSE") ||
                    pn.equals("SunEC"))) ||
                (mn.equals("jdk.crypto.mscapi") && pn.equals("SunMSCAPI")) ||
                (mn.equals("jdk.crypto.cryptoki") &&
                    pn.startsWith("SunPKCS11")));
        } else {
            return false;
        }
    }

    private static ConcurrentHashMap<String, String> collectOIDAliases() {
        ConcurrentHashMap<String, String> t = new ConcurrentHashMap<>();
        for (Provider provider : Security.getProviders()) {
            // skip providers which are already using SecurityProviderConstants
            // and KnownOIDs
            if (isKnownProvider(provider)) {
                continue;
            }
            for (Object key : provider.keySet()) {
                String alias = (String)key;
                String upperCaseAlias = alias.toUpperCase(Locale.ENGLISH);
                int index;
                if (upperCaseAlias.startsWith("ALG.ALIAS") &&
                    (index = upperCaseAlias.indexOf("OID.")) != -1) {
                    index += "OID.".length();
                    if (index == alias.length()) {
                        // invalid alias entry
                        break;
                    }
                    String ostr = alias.substring(index);
                    String stdAlgName = provider.getProperty(alias);
                    if (stdAlgName != null) {
                        String upperStdAlgName = stdAlgName.toUpperCase(Locale.ENGLISH);
                        // add the name->oid and oid->name mappings if none exists
                        if (KnownOIDs.findMatch(upperStdAlgName) == null) {
                            // do not override earlier entries if it exists
                            t.putIfAbsent(upperStdAlgName, ostr);
                        }
                        if (KnownOIDs.findMatch(ostr) == null) {
                            // do not override earlier entries if it exists
                            t.putIfAbsent(ostr, stdAlgName);
                        }
                    }
                }
            }
        }
        return t;
    }

    public static final ObjectIdentifier MD2_oid =
            ObjectIdentifier.of(KnownOIDs.MD2);

    public static final ObjectIdentifier MD5_oid =
            ObjectIdentifier.of(KnownOIDs.MD5);

    public static final ObjectIdentifier SHA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA_1);

    public static final ObjectIdentifier SHA224_oid =
            ObjectIdentifier.of(KnownOIDs.SHA_224);

    public static final ObjectIdentifier SHA256_oid =
            ObjectIdentifier.of(KnownOIDs.SHA_256);

    public static final ObjectIdentifier SHA384_oid =
            ObjectIdentifier.of(KnownOIDs.SHA_384);

    public static final ObjectIdentifier SHA512_oid =
            ObjectIdentifier.of(KnownOIDs.SHA_512);

    public static final ObjectIdentifier SHA512_224_oid =
            ObjectIdentifier.of(KnownOIDs.SHA_512$224);

    public static final ObjectIdentifier SHA512_256_oid =
            ObjectIdentifier.of(KnownOIDs.SHA_512$256);

    public static final ObjectIdentifier SHA3_224_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_224);

    public static final ObjectIdentifier SHA3_256_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_256);

    public static final ObjectIdentifier SHA3_384_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_384);

    public static final ObjectIdentifier SHA3_512_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_512);

    public static final ObjectIdentifier DSA_oid =
            ObjectIdentifier.of(KnownOIDs.DSA);

    public static final ObjectIdentifier EC_oid =
            ObjectIdentifier.of(KnownOIDs.EC);

    public static final ObjectIdentifier RSAEncryption_oid =
            ObjectIdentifier.of(KnownOIDs.RSA);

    public static final ObjectIdentifier RSASSA_PSS_oid =
            ObjectIdentifier.of(KnownOIDs.RSASSA_PSS);

    public static final ObjectIdentifier MGF1_oid =
            ObjectIdentifier.of(KnownOIDs.MGF1);

    public static final ObjectIdentifier SHA1withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA1withRSA);
    public static final ObjectIdentifier SHA224withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA224withRSA);
    public static final ObjectIdentifier SHA256withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA256withRSA);
    public static final ObjectIdentifier SHA384withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA384withRSA);
    public static final ObjectIdentifier SHA512withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA512withRSA);
    public static final ObjectIdentifier SHA512$224withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA512$224withRSA);
    public static final ObjectIdentifier SHA512$256withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA512$256withRSA);
    public static final ObjectIdentifier MD2withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.MD2withRSA);
    public static final ObjectIdentifier MD5withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.MD5withRSA);
    public static final ObjectIdentifier SHA3_224withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_224withRSA);
    public static final ObjectIdentifier SHA3_256withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_256withRSA);
    public static final ObjectIdentifier SHA3_384withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_384withRSA);
    public static final ObjectIdentifier SHA3_512withRSA_oid =
            ObjectIdentifier.of(KnownOIDs.SHA3_512withRSA);
}
