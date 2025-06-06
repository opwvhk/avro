/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Compiler.hh"
#include "GenericDatum.hh"
#include "NodeImpl.hh"
#include "ValidSchema.hh"

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>
#include <boost/test/unit_test.hpp>

namespace avro {
namespace schema {

const char *basicSchemas[] = {
    R"("null")",
    R"("boolean")",
    R"("int")",
    R"("long")",
    R"("float")",
    R"("double")",
    R"("bytes")",
    R"("string")",

    // Primitive types - longer
    R"({ "type": "null" })",
    R"({ "type": "boolean" })",
    R"({ "type": "int" })",
    R"({ "type": "long" })",
    R"({ "type": "float" })",
    R"({ "type": "double" })",
    R"({ "type": "bytes" })",
    R"({ "type": "string" })",

    // Record
    R"({
        "type":"record",
        "name":"Test",
        "doc":"Doc_string",
        "fields":[]
    })",
    R"({
        "type":"record",
        "name":"Test",
        "fields": [
            {"name":"f","type":"long"}
        ]
    })",
    R"({
        "type":"record",
        "name":"Test",
        "fields":[
            {"name":"f1","type":"long","doc":"field_doc"},
            {"name":"f2","type":"int"}
        ]
    })",
    R"({
        "type":"error",
        "name":"Test",
        "fields":[
            {"name":"f1","type":"long"},
            {"name":"f2","type":"int"}
        ]
    })",
    // Recursive.
    R"({
        "type":"record",
        "name":"LongList",
        "fields":[
            {"name":"value","type":"long","doc":"recursive_doc"},
            {"name":"next","type":["LongList","null"]}
        ]
    })",

    // Enum
    R"({
        "type":"enum",
        "doc":"enum_doc",
        "name":"Test",
        "symbols":["A","B"]
    })",

    // Array
    R"({
        "type":"array",
        "doc":"array_doc",
        "items":"long"
    })",
    R"({
        "type":"array",
        "items":{
            "type":"enum",
            "name":"Test",
            "symbols":["A","B"]
        }
    })",

    // Map
    R"({"type":"map","doc":"map_doc","values":"long"})",
    R"({
        "type":"map",
        "values":{
            "type":"enum",
            "name":"Test",
            "symbols":["A","B"]
        }
    })",

    // Union
    R"(["string","null","long"])",

    // Fixed
    R"({"type":"fixed","doc":"fixed_doc","name":"Test","size":1})",
    R"({"type":"fixed","name":"MyFixed","namespace":"org.apache.hadoop.avro","size":1})",
    R"({"type":"fixed","name":"Test","size":1})",
    R"({"type":"fixed","name":"Test","size":1})",

    // Extra attributes (should be ignored)
    R"({"type": "null", "extra attribute": "should be ignored"})",
    R"({"type": "boolean", "extra1": 1, "extra2": 2, "extra3": 3})",
    R"({
        "type": "record",
        "name": "Test",
        "fields":[
            {"name": "f","type":"long"}
        ],
        "extra attribute": 1
    })",
    R"({"type": "enum", "name": "Test", "symbols": ["A", "B"],"extra attribute": 1})",
    R"({"type": "array", "items": "long", "extra attribute": "1"})",
    R"({"type": "array", "items": "long", "extra attribute": 1})",
    R"({"type": "array", "items": "long", "extra attribute": true})",
    R"({"type": "array", "items": "long", "extra attribute": 1.1})",
    R"({"type": "array", "items": "long", "extra attribute": {"extra extra attribute": "1"}})",
    R"({"type": "map", "values": "long", "extra attribute": 1})",
    R"({"type": "fixed", "name": "Test", "size": 1, "extra attribute": 1})",

    // defaults
    // default double -  long
    R"({ "name":"test", "type": "record", "fields": [ {"name": "double","type": "double","default" : 2 }]})",
    // default double - double
    R"({ "name":"test", "type": "record", "fields": [ {"name": "double","type": "double","default" : 1.2 }]})",

    // namespace with '$' in it.
    R"({
        "type":"record",
        "name":"Test",
        "namespace":"a.b$",
        "fields":[
            {"name":"f","type":"long"}
        ]
    })",

    // Custom attribute(s) for field in record
    R"({
        "type": "record",
        "name": "Test",
        "fields":[
            {"name": "f1","type": "long","extra field": "1"}
        ]
    })",
    R"({
        "type": "record",
        "name": "Test",
        "fields":[
            {"name": "f1","type": "long","extra field1": "1","extra field2": "2"}
        ]
    })"};

const char *basicSchemaErrors[] = {
    // Record
    // No fields
    R"({"type":"record","name":"LongList"})",
    // Fields not an array
    R"({"type":"record","name":"LongList", "fields": "hi"})",

    // Undefined name
    R"({
        "type":"record",
        "name":"LongList",
        "fields":[
            {"name":"value","type":"long"},
            {"name":"next","type":["LongListA","null"]}
        ]
    })",

    // Enum
    // Symbols not an array
    R"({"type": "enum", "name": "Status", "symbols":"Normal Caution Critical"})",
    // Name not a string
    R"({"type": "enum", "name": [ 0, 1, 1, 2, 3, 5, 8 ], "symbols": ["Golden", "Mean"]})",
    // No name
    R"({"type": "enum", "symbols" : ["I", "will", "fail", "no", "name"]})",
    // Duplicate symbol
    R"({"type": "enum", "name": "Test", "symbols" : ["AA", "AA"]})",

    // Union
    // Duplicate type
    R"(["string", "long", "long"])",
    // Duplicate type
    R"([
        {"type": "array", "items": "long"},
        {"type": "array", "items": "string"}
    ])",

    // Fixed
    // No size
    R"({"type": "fixed", "name": "Missing size"})",
    // No name
    R"({"type": "fixed", "size": 314})",

    // defaults
    // default double - null
    R"({ "name":"test", "type": "record", "fields": [ {"name": "double","type": "double","default" : null }]})",
    // default double - string
    R"({ "name":"test", "type": "record", "fields": [ {"name": "double","type": "double","default" : "string" }]})"

};

const char *roundTripSchemas[] = {
    R"("null")",
    R"("boolean")",
    R"("int")",
    R"("long")",
    R"("float")",
    R"("double")",
    R"("bytes")",
    R"("string")",

    // Record
    R"({"type":"record","name":"Test","fields":[]})",
    R"({
        "type":"record",
        "name":"Test",
        "fields":[
            {"name":"f","type":"long"}
        ]
    })",
    R"({
        "type":"record",
        "name":"Test",
        "fields":[
            {"name":"f1","type":"long"},
            {"name":"f2","type":"int"}
        ]
    })",

    /* Avro-C++ cannot do a round-trip on error schemas.
     * R"({
     *      "type":"error",
     *      "name":"Test",
     *      "fields":[
     *          {"name":"f1","type":"long"},
     *          {"name":"f2","type":"int"}
     *          ]
     * })",
     */

    // Recursive.
    R"({
        "type":"record",
        "name":"LongList",
        "fields":[
            {"name":"value","type":"long"},
            {"name":"next","type":["LongList","null"]}
        ]
    })",

    // Enum
    R"({"type":"enum","name":"Test","symbols":["A","B"]})",

    // Array
    R"({"type":"array","items":"long"})",
    R"({
        "type":"array",
        "items":{
            "type":"enum",
            "name":"Test",
            "symbols":["A","B"]
        }
    })",

    // Map
    R"({"type":"map","values":"long"})",
    R"({
        "type":"map",
        "values":{
            "type":"enum",
            "name":"Test",
            "symbols":["A","B"]
        }
    })",

    // Union
    R"(["string","null","long"])",

    // Fixed
    R"({"type":"fixed","name":"Test","size":1})",
    R"({"type":"fixed","namespace":"org.apache.hadoop.avro","name":"MyFixed","size":1})",
    R"({"type":"fixed","name":"Test","size":1})",
    R"({"type":"fixed","name":"Test","size":1})",

    // Logical types
    R"({"type":"bytes","logicalType":"big-decimal"})",
    R"({"type":"bytes","logicalType":"decimal","precision":12,"scale":6})",
    R"({"type":"fixed","name":"test","size":16,"logicalType":"decimal","precision":38,"scale":9})",
    R"({"type":"fixed","name":"test","size":129,"logicalType":"decimal","precision":310,"scale":155})",
    R"({"type":"int","logicalType":"date"})",
    R"({"type":"int","logicalType":"time-millis"})",
    R"({"type":"long","logicalType":"time-micros"})",
    R"({"type":"long","logicalType":"timestamp-millis"})",
    R"({"type":"long","logicalType":"timestamp-micros"})",
    R"({"type":"long","logicalType":"timestamp-nanos"})",
    R"({"type":"long","logicalType":"local-timestamp-millis"})",
    R"({"type":"long","logicalType":"local-timestamp-micros"})",
    R"({"type":"long","logicalType":"local-timestamp-nanos"})",
    R"({"type":"fixed","name":"test","size":12,"logicalType":"duration"})",
    R"({"type":"string","logicalType":"uuid"})",
    R"({"type":"fixed","name":"test","size":16,"logicalType":"uuid"})",

    // namespace with '$' in it.
    R"({
        "type":"record",
        "namespace":"a.b$",
        "name":"Test",
        "fields":[
            {"name":"f","type":"long"}
        ]
    })",

    // Custom fields
    R"({
        "type":"record",
        "name":"Test",
        "fields":[
            {"name":"f1","type":"long","extra_field":"1"},
            {"name":"f2","type":"int"}
        ]
    })",
    R"({
        "type":"record",
        "name":"Test",
        "fields":[
            {"name":"f1","type":"long","extra_field":"1"},
            {"name":"f2","type":"int","extra_field1":"21","extra_field2":"22"}
        ]
    })",
    R"({"type":"array","items":"long","extra":"1"})",
    R"({"type":"map","values":"long","extra":"1"})",
    R"({"type":"fixed","name":"Test","size":1,"extra":"1"})",
    R"({"type":"enum","name":"Test","symbols":["A","B"],"extra":"1"})",
};

const char *malformedLogicalTypes[] = {
    // Wrong base type.
    R"({"type":"long","logicalType": "big-decimal"})",
    R"({"type":"long","logicalType": "decimal","precision": 10})",
    R"({"type":"string","logicalType":"date"})",
    R"({"type":"string","logicalType":"time-millis"})",
    R"({"type":"string","logicalType":"time-micros"})",
    R"({"type":"string","logicalType":"timestamp-millis"})",
    R"({"type":"string","logicalType":"timestamp-micros"})",
    R"({"type":"string","logicalType":"timestamp-nanos"})",
    R"({"type":"string","logicalType":"local-timestamp-millis"})",
    R"({"type":"string","logicalType":"local-timestamp-micros"})",
    R"({"type":"string","logicalType":"local-timestamp-nanos"})",
    R"({"type":"string","logicalType":"duration"})",
    R"({"type":"long","logicalType":"uuid"})",
    // Missing the required field 'precision'.
    R"({"type":"bytes","logicalType":"decimal"})",
    // The claimed precision is not supported by the size of the fixed type.
    R"({"type":"fixed","logicalType":"decimal","size":4,"name":"a","precision":20})",
    R"({"type":"fixed","logicalType":"decimal","size":129,"name":"a","precision":311})",
    // Scale is larger than precision.
    R"({"type":"bytes","logicalType":"decimal","precision":5,"scale":10})",
    // Precision is not supported by the big-decimal logical type
    // and scale is integrated in bytes.
    R"({"type":"bytes","logicalType": "big-decimal","precision": 9})",
    R"({"type":"bytes","logicalType": "big-decimal","scale": 2})",
    R"({"type":"bytes","logicalType": "big-decimal","precision": 9,"scale": 2})",
    R"({"type":"fixed","logicalType":"uuid","size":12,"name":"invalid_uuid_size"})",
};
const char *schemasToCompact[] = {
    // Schema without any whitespace
    R"({"type":"record","name":"Test","fields":[]})",

    // Schema with whitespaces outside of field names/values only.
    "{\"type\":   \"record\",\n   \n\"name\":\"Test\", \t\t\"fields\":[]}\n \n",

    // Schema with whitespaces both inside and outside of field names/values.
    "{\"type\":   \"record\",  \"name\":               \"ComplexInteger\"\n, "
    "\"doc\": \"record_doc °C \u00f8 \x1f \\n \n \t\", "
    "\"fields\": ["
    "{\"name\":   \"re1\", \"type\":               \"long\", "
    "\"doc\":   \"A \\\"quoted doc\\\"\"      },                 "
    "{\"name\":  \"re2\", \"type\":   \"long\", \n\t"
    "\"doc\": \"extra slashes\\\\\\\\\"}"
    "]}"};

const char *compactSchemas[] = {
    R"({"type":"record","name":"Test","fields":[]})",
    R"({"type":"record","name":"Test","fields":[]})",
    "{\"type\":\"record\",\"name\":\"ComplexInteger\","
    "\"doc\":\"record_doc °C \u00f8 \\u001f \\n \\n \\t\","
    "\"fields\":["
    "{\"name\":\"re1\",\"type\":\"long\",\"doc\":\"A \\\"quoted doc\\\"\"},"
    "{\"name\":\"re2\",\"type\":\"long\",\"doc\":\"extra slashes\\\\\\\\\"}"
    "]}"};

static const std::vector<char> whitespaces = {' ', '\f', '\n', '\r', '\t', '\v'};

static std::string removeWhitespaceFromSchema(const std::string &schema) {
    std::string trimmedSchema = schema;
    for (char toReplace : whitespaces) {
        boost::algorithm::replace_all(trimmedSchema, std::string{toReplace}, "");
    }
    return trimmedSchema;
}

void testTypes() {
    BOOST_CHECK_EQUAL(isAvroType(AVRO_BOOL), true);
}

static void testBasic(const char *schema) {
    BOOST_TEST_CHECKPOINT(schema);
    compileJsonSchemaFromString(schema);
}

static void testBasic_fail(const char *schema) {
    BOOST_TEST_CHECKPOINT(schema);
    BOOST_CHECK_THROW(compileJsonSchemaFromString(schema), Exception);
}

static void testCompile(const char *schema) {
    BOOST_TEST_CHECKPOINT(schema);
    compileJsonSchemaFromString(std::string(schema));
}

// Test that the JSON output from a valid schema matches the JSON that was
// used to construct it, apart from whitespace changes.
static void testRoundTrip(const char *schema) {
    BOOST_TEST_CHECKPOINT(schema);
    avro::ValidSchema compiledSchema =
        compileJsonSchemaFromString(std::string(schema));
    std::ostringstream os;
    compiledSchema.toJson(os);
    std::string result = removeWhitespaceFromSchema(os.str());
    std::string trimmedSchema = removeWhitespaceFromSchema(schema);
    BOOST_CHECK_EQUAL(result, trimmedSchema);
    // Verify that the compact schema from toJson has the same content as the
    // schema.
    std::string result2 = compiledSchema.toJson(false);
    BOOST_CHECK_EQUAL(result2, trimmedSchema);
}

static void testCompactSchemas() {
    for (size_t i = 0; i < sizeof(schemasToCompact) / sizeof(schemasToCompact[0]); i++) {
        const char *schema = schemasToCompact[i];
        BOOST_TEST_CHECKPOINT(schema);
        avro::ValidSchema compiledSchema =
            compileJsonSchemaFromString(std::string(schema));

        std::string result = compiledSchema.toJson(false);
        BOOST_CHECK_EQUAL(result, compactSchemas[i]);
    }
}

static void testLogicalTypes() {
    const char *bytesBigDecimalType = R"({
        "type": "bytes",
        "logicalType": "big-decimal"
    })";
    const char *bytesDecimalType = R"({
        "type": "bytes",
        "logicalType": "decimal",
        "precision": 10,
        "scale": 2
    })";
    const char *fixedDecimalType = R"({
        "type": "fixed",
        "size": 16,
        "name": "fixedDecimalType",
        "logicalType": "decimal",
        "precision": 12,
        "scale": 6
    })";
    const char *dateType = R"({"type": "int", "logicalType": "date"})";
    const char *timeMillisType = R"({"type": "int", "logicalType": "time-millis"})";
    const char *timeMicrosType = R"({"type": "long", "logicalType": "time-micros"})";
    const char *timestampMillisType = R"({"type": "long", "logicalType": "timestamp-millis"})";
    const char *timestampMicrosType = R"({"type": "long", "logicalType": "timestamp-micros"})";
    const char *timestampNanosType = R"({"type": "long", "logicalType": "timestamp-nanos"})";
    const char *localTimestampMillisType = R"({"type": "long", "logicalType": "local-timestamp-millis"})";
    const char *localTimestampMicrosType = R"({"type": "long", "logicalType": "local-timestamp-micros"})";
    const char *localTimestampNanosType = R"({"type": "long", "logicalType": "local-timestamp-nanos"})";
    const char *durationType = R"({"type": "fixed","size": 12,"name": "durationType","logicalType": "duration"})";
    const char *uuidStringType = R"({"type": "string","logicalType": "uuid"})";
    const char *uuidFixedType = R"({"type": "fixed", "size": 16, "name": "uuidFixedType", "logicalType": "uuid"})";
    // AVRO-2923 Union with LogicalType
    const char *unionType = R"([{"type":"string", "logicalType":"uuid"},"null"]})";
    {
        BOOST_TEST_CHECKPOINT(bytesBigDecimalType);
        ValidSchema schema = compileJsonSchemaFromString(bytesBigDecimalType);
        BOOST_CHECK(schema.root()->type() == AVRO_BYTES);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::BIG_DECIMAL);
    }
    {
        BOOST_TEST_CHECKPOINT(bytesDecimalType);
        ValidSchema schema1 = compileJsonSchemaFromString(bytesDecimalType);
        BOOST_CHECK(schema1.root()->type() == AVRO_BYTES);
        LogicalType logicalType = schema1.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::DECIMAL);
        BOOST_CHECK(logicalType.precision() == 10);
        BOOST_CHECK(logicalType.scale() == 2);

        BOOST_TEST_CHECKPOINT(fixedDecimalType);
        ValidSchema schema2 = compileJsonSchemaFromString(fixedDecimalType);
        BOOST_CHECK(schema2.root()->type() == AVRO_FIXED);
        logicalType = schema2.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::DECIMAL);
        BOOST_CHECK(logicalType.precision() == 12);
        BOOST_CHECK(logicalType.scale() == 6);

        GenericDatum bytesDatum(schema1);
        BOOST_CHECK(bytesDatum.logicalType().type() == LogicalType::DECIMAL);
        GenericDatum fixedDatum(schema2);
        BOOST_CHECK(fixedDatum.logicalType().type() == LogicalType::DECIMAL);
    }
    {
        BOOST_TEST_CHECKPOINT(dateType);
        ValidSchema schema = compileJsonSchemaFromString(dateType);
        BOOST_CHECK(schema.root()->type() == AVRO_INT);
        BOOST_CHECK(schema.root()->logicalType().type() == LogicalType::DATE);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::DATE);
    }
    {
        BOOST_TEST_CHECKPOINT(timeMillisType);
        ValidSchema schema = compileJsonSchemaFromString(timeMillisType);
        BOOST_CHECK(schema.root()->type() == AVRO_INT);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::TIME_MILLIS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::TIME_MILLIS);
    }
    {
        BOOST_TEST_CHECKPOINT(timeMicrosType);
        ValidSchema schema = compileJsonSchemaFromString(timeMicrosType);
        BOOST_CHECK(schema.root()->type() == AVRO_LONG);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::TIME_MICROS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::TIME_MICROS);
    }
    {
        BOOST_TEST_CHECKPOINT(timestampMillisType);
        ValidSchema schema = compileJsonSchemaFromString(timestampMillisType);
        BOOST_CHECK(schema.root()->type() == AVRO_LONG);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::TIMESTAMP_MILLIS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::TIMESTAMP_MILLIS);
    }
    {
        BOOST_TEST_CHECKPOINT(timestampMicrosType);
        ValidSchema schema = compileJsonSchemaFromString(timestampMicrosType);
        BOOST_CHECK(schema.root()->type() == AVRO_LONG);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::TIMESTAMP_MICROS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::TIMESTAMP_MICROS);
    }
    {
        BOOST_TEST_CHECKPOINT(timestampNanosType);
        ValidSchema schema = compileJsonSchemaFromString(timestampNanosType);
        BOOST_CHECK(schema.root()->type() == AVRO_LONG);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::TIMESTAMP_NANOS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::TIMESTAMP_NANOS);
    }
    {
        BOOST_TEST_CHECKPOINT(localTimestampMillisType);
        ValidSchema schema = compileJsonSchemaFromString(localTimestampMillisType);
        BOOST_CHECK(schema.root()->type() == AVRO_LONG);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::LOCAL_TIMESTAMP_MILLIS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::LOCAL_TIMESTAMP_MILLIS);
    }
    {
        BOOST_TEST_CHECKPOINT(localTimestampMicrosType);
        ValidSchema schema = compileJsonSchemaFromString(localTimestampMicrosType);
        BOOST_CHECK(schema.root()->type() == AVRO_LONG);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::LOCAL_TIMESTAMP_MICROS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::LOCAL_TIMESTAMP_MICROS);
    }
    {
        BOOST_TEST_CHECKPOINT(localTimestampNanosType);
        ValidSchema schema = compileJsonSchemaFromString(localTimestampNanosType);
        BOOST_CHECK(schema.root()->type() == AVRO_LONG);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::LOCAL_TIMESTAMP_NANOS);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::LOCAL_TIMESTAMP_NANOS);
    }
    {
        BOOST_TEST_CHECKPOINT(durationType);
        ValidSchema schema = compileJsonSchemaFromString(durationType);
        BOOST_CHECK(schema.root()->type() == AVRO_FIXED);
        BOOST_CHECK(schema.root()->fixedSize() == 12);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::DURATION);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::DURATION);
    }
    {
        BOOST_TEST_CHECKPOINT(uuidStringType);
        ValidSchema schema = compileJsonSchemaFromString(uuidStringType);
        BOOST_CHECK(schema.root()->type() == AVRO_STRING);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::UUID);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::UUID);
    }
    {
        BOOST_TEST_CHECKPOINT(uuidFixedType);
        ValidSchema schema = compileJsonSchemaFromString(uuidFixedType);
        BOOST_CHECK(schema.root()->type() == AVRO_FIXED);
        BOOST_CHECK(schema.root()->fixedSize() == 16);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::UUID);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::UUID);
    }
    {
        BOOST_TEST_CHECKPOINT(unionType);
        ValidSchema schema = compileJsonSchemaFromString(unionType);
        BOOST_CHECK(schema.root()->type() == AVRO_UNION);
        LogicalType logicalType = schema.root()->logicalType();
        BOOST_CHECK(logicalType.type() == LogicalType::NONE);
        GenericDatum datum(schema);
        BOOST_CHECK(datum.logicalType().type() == LogicalType::UUID);
    }
}

static void testMalformedLogicalTypes(const char *schema) {
    BOOST_TEST_CHECKPOINT(schema);
    ValidSchema parsedSchema = compileJsonSchemaFromString(schema);
    LogicalType logicalType = parsedSchema.root()->logicalType();
    BOOST_CHECK(logicalType.type() == LogicalType::NONE);
    GenericDatum datum(parsedSchema);
    BOOST_CHECK(datum.logicalType().type() == LogicalType::NONE);
}

static void testCustomLogicalType() {
    // Declare a custom logical type.
    struct MapLogicalType : public CustomLogicalType {
        MapLogicalType() : CustomLogicalType("map") {}
    };

    // Register the custom logical type with the registry.
    CustomLogicalTypeRegistry::instance().registerType("map", [](const std::string &) {
        return std::make_shared<MapLogicalType>();
    });

    auto verifyCustomLogicalType = [](const ValidSchema &schema) {
        auto logicalType = schema.root()->logicalType();
        BOOST_CHECK_EQUAL(logicalType.type(), LogicalType::CUSTOM);
        BOOST_CHECK_EQUAL(logicalType.customLogicalType()->name(), "map");
    };

    const std::string schema =
        R"({ "type": "array",
             "logicalType": "map",
             "items": {
               "type": "record",
               "name": "k12_v13",
               "fields": [
                 { "name": "key", "type": "int", "field-id": 12 },
                 { "name": "value", "type": "string", "field-id": 13 }
               ]
             }
           })";
    auto compiledSchema = compileJsonSchemaFromString(schema);
    verifyCustomLogicalType(compiledSchema);

    auto json = compiledSchema.toJson();
    auto parsedSchema = compileJsonSchemaFromString(json);
    verifyCustomLogicalType(parsedSchema);
}

static void testParseCustomAttributes() {
    const std::string schema = R"({
        "type": "record",
        "name": "my_record",
        "fields": [
            { "name": "long_field",
              "type": ["null", "long"],
              "field-id": 1 },
            { "name": "array_field",
              "type": { "type": "array", "items": "int", "element-id": 3 },
              "field-id": 2,
              "extra": "1", "extra2": "2" },
            { "name": "map_field",
              "type": { "type": "map", "values": "int", "key-id": 5, "value-id": 6 },
              "field-id": 4,
              "extra": "foo" },
            { "name": "timestamp_field",
              "type": "long", "logicalType": "timestamp-micros", "adjust-to-utc": true,
              "field-id": 10,
              "extra": "bar" },
            { "name": "no_custom_attributes_field",
              "type": "long" }
        ]
    })";

    ValidSchema compiledSchema = compileJsonSchemaFromString(schema);
    const NodePtr &root = compiledSchema.root();
    BOOST_CHECK_EQUAL(root->customAttributes(), 5);

    // long_field
    {
        auto customAttributes = root->customAttributesAt(0);
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("field-id").value(), "1");
    }

    // array_field
    {
        auto customAttributes = root->customAttributesAt(1);
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("extra").value(), "1");
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("extra2").value(), "2");
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("field-id").value(), "2");

        auto arrayField = root->leafAt(1);
        BOOST_CHECK_EQUAL(arrayField->customAttributes(), 1);
        auto arrayFieldCustomAttributes = arrayField->customAttributesAt(0);
        BOOST_CHECK_EQUAL(arrayFieldCustomAttributes.getAttribute("element-id").value(), "3");
    }

    // map_field
    {
        auto customAttributes = root->customAttributesAt(2);
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("field-id").value(), "4");
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("extra").value(), "foo");

        auto mapField = root->leafAt(2);
        BOOST_CHECK_EQUAL(mapField->customAttributes(), 1);
        auto mapFieldCustomAttributes = mapField->customAttributesAt(0);
        BOOST_CHECK_EQUAL(mapFieldCustomAttributes.getAttribute("key-id").value(), "5");
        BOOST_CHECK_EQUAL(mapFieldCustomAttributes.getAttribute("value-id").value(), "6");
    }

    // timestamp_field
    {
        auto customAttributes = root->customAttributesAt(3);
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("field-id").value(), "10");
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("extra").value(), "bar");
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("adjust-to-utc").value(), "true");
    }

    // no_custom_attributes_field
    {
        auto customAttributes = root->customAttributesAt(4);
        BOOST_CHECK_EQUAL(customAttributes.attributes().size(), 0);
    }
}

static void testAddCustomAttributes() {
    auto recordNode = std::make_shared<NodeRecord>();

    // long_field
    {
        CustomAttributes customAttributes;
        customAttributes.addAttribute("field-id", "1");
        recordNode->addCustomAttributesForField(customAttributes);
        recordNode->addLeaf(std::make_shared<NodePrimitive>(AVRO_LONG));
        recordNode->addName("long_field");
    }

    // array_field
    {
        auto arrayField = std::make_shared<NodeArray>(SingleLeaf(std::make_shared<NodePrimitive>(AVRO_INT)));
        CustomAttributes elementCustomAttributes;
        elementCustomAttributes.addAttribute("element-id", "3");
        arrayField->addCustomAttributesForField(elementCustomAttributes);

        CustomAttributes customAttributes;
        customAttributes.addAttribute("field-id", "2");
        customAttributes.addAttribute("extra", "1");
        customAttributes.addAttribute("extra2", "2");
        recordNode->addCustomAttributesForField(customAttributes);
        recordNode->addLeaf(arrayField);
        recordNode->addName("array_field");
    }

    // map_field
    {
        auto mapField = std::make_shared<NodeMap>(SingleLeaf(std::make_shared<NodePrimitive>(AVRO_INT)));
        CustomAttributes keyValueCustomAttributes;
        keyValueCustomAttributes.addAttribute("key-id", "5");
        keyValueCustomAttributes.addAttribute("value-id", "6");
        mapField->addCustomAttributesForField(keyValueCustomAttributes);

        CustomAttributes customAttributes;
        customAttributes.addAttribute("field-id", "4");
        customAttributes.addAttribute("extra", "foo");
        recordNode->addCustomAttributesForField(customAttributes);
        recordNode->addLeaf(mapField);
        recordNode->addName("map_field");
    }

    // timestamp_field
    {
        auto timestampField = std::make_shared<NodePrimitive>(AVRO_LONG);
        CustomAttributes customAttributes;
        customAttributes.addAttribute("field-id", "10");
        customAttributes.addAttribute("extra", "bar");
        customAttributes.addAttribute("adjust-to-utc", "true");
        recordNode->addCustomAttributesForField(customAttributes);
        recordNode->addLeaf(timestampField);
        recordNode->addName("timestamp_field");
    }

    const std::string expected = R"({
        "type": "record",
        "name": "",
        "fields": [
            { "name": "long_field",
              "type": "long",
              "field-id": "1" },
            { "name": "array_field",
              "type": { "type": "array", "items": "int", "element-id": "3" },
              "extra": "1",
              "extra2": "2",
              "field-id": "2" },
            { "name": "map_field",
              "type": { "type": "map", "values": "int", "key-id": "5", "value-id": "6" },
              "extra": "foo",
              "field-id": "4" },
            { "name": "timestamp_field",
              "type": "long",
              "adjust-to-utc": "true",
              "extra": "bar",
              "field-id": "10" }
        ]
    })";
    ValidSchema schema(recordNode);
    std::string json = schema.toJson();
    BOOST_CHECK_EQUAL(removeWhitespaceFromSchema(json), removeWhitespaceFromSchema(expected));
}

static void testCustomAttributesJson2Schema2Json() {
    const std::string schema = R"({
        "type": "record",
        "name": "my_record",
        "fields": [
            { "name": "long_field", "type": "long", "int_key": 1, "str_key": "1" }
        ]
    })";
    ValidSchema compiledSchema = compileJsonSchemaFromString(schema);

    // Verify custom attributes from parsed schema
    auto customAttributes = compiledSchema.root()->customAttributesAt(0);
    BOOST_CHECK_EQUAL(customAttributes.getAttribute("int_key").value(), "1");
    BOOST_CHECK_EQUAL(customAttributes.getAttribute("str_key").value(), "1");

    // Verify custom attributes from json result
    std::string json = compiledSchema.toJson();
    BOOST_CHECK_EQUAL(removeWhitespaceFromSchema(json), removeWhitespaceFromSchema(schema));
}

static void testCustomAttributesSchema2Json2Schema() {
    const std::string expected = R"({
        "type": "record",
        "name": "my_record",
        "fields": [
            { "name": "long_field", "type": "long", "int_key": 1, "str_key": "1" }
        ]
    })";

    auto recordNode = std::make_shared<NodeRecord>();
    {
        CustomAttributes customAttributes;
        customAttributes.addAttribute("int_key", "1", /*addQuotes=*/false);
        customAttributes.addAttribute("str_key", "1", /*addQuotes=*/true);
        recordNode->addCustomAttributesForField(customAttributes);
        recordNode->addLeaf(std::make_shared<NodePrimitive>(AVRO_LONG));
        recordNode->addName("long_field");
        recordNode->setName(Name("my_record"));
    }

    // Verify custom attributes from json result
    ValidSchema schema(recordNode);
    std::string json = schema.toJson();
    BOOST_CHECK_EQUAL(removeWhitespaceFromSchema(json), removeWhitespaceFromSchema(expected));

    // Verify custom attributes from parsed schema
    {
        auto parsedSchema = compileJsonSchemaFromString(json);
        auto customAttributes = parsedSchema.root()->customAttributesAt(0);
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("int_key").value(), "1");
        BOOST_CHECK_EQUAL(customAttributes.getAttribute("str_key").value(), "1");
    }
}

} // namespace schema
} // namespace avro

#define ENDOF(x) (x + sizeof(x) / sizeof(x[0]))

#define ADD_PARAM_TEST(ts, func, data) \
    ts->add(BOOST_PARAM_TEST_CASE(&func, data, ENDOF(data)))

boost::unit_test::test_suite *
init_unit_test_suite(int /*argc*/, char * /*argv*/[]) {
    using namespace boost::unit_test;

    auto *ts = BOOST_TEST_SUITE("Avro C++ unit tests for schemas");
    ts->add(BOOST_TEST_CASE(&avro::schema::testTypes));
    ADD_PARAM_TEST(ts, avro::schema::testBasic, avro::schema::basicSchemas);
    ADD_PARAM_TEST(ts, avro::schema::testBasic_fail,
                   avro::schema::basicSchemaErrors);
    ADD_PARAM_TEST(ts, avro::schema::testCompile, avro::schema::basicSchemas);
    ADD_PARAM_TEST(ts, avro::schema::testRoundTrip, avro::schema::roundTripSchemas);
    ts->add(BOOST_TEST_CASE(&avro::schema::testLogicalTypes));
    ADD_PARAM_TEST(ts, avro::schema::testMalformedLogicalTypes,
                   avro::schema::malformedLogicalTypes);
    ts->add(BOOST_TEST_CASE(&avro::schema::testCompactSchemas));
    ts->add(BOOST_TEST_CASE(&avro::schema::testCustomLogicalType));
    ts->add(BOOST_TEST_CASE(&avro::schema::testParseCustomAttributes));
    ts->add(BOOST_TEST_CASE(&avro::schema::testAddCustomAttributes));
    ts->add(BOOST_TEST_CASE(&avro::schema::testCustomAttributesJson2Schema2Json));
    ts->add(BOOST_TEST_CASE(&avro::schema::testCustomAttributesSchema2Json2Schema));
    return ts;
}
