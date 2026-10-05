#include <kitzoo/utilities/uuid.hpp>

#include <gtest/gtest.h>
#include <unordered_set>

TEST(UuidTest, GeneratesVersionFourUuid) {
  auto const uuid = kitzoo::util::Uuid::random();
  auto const text = uuid.to_string();
  EXPECT_EQ(text.size(), 36u);
  EXPECT_EQ(text[14], '4');
  EXPECT_TRUE(text[19] == '8' || text[19] == '9' || text[19] == 'a' || text[19] == 'b');
  EXPECT_FALSE(uuid.is_nil());
}

TEST(UuidTest, ParsesAndFormatsCanonicalText) {
  auto parsed = kitzoo::util::Uuid::parse("550E8400-E29B-41D4-A716-446655440000");
  ASSERT_TRUE(parsed);
  EXPECT_EQ(parsed.value().to_string(), "550e8400-e29b-41d4-a716-446655440000");
  EXPECT_EQ(kitzoo::util::Uuid::parse(parsed.value().to_string()).value(), parsed.value());
}

TEST(UuidTest, RejectsInvalidText) {
  EXPECT_FALSE(kitzoo::util::Uuid::parse("not-a-uuid"));
  EXPECT_FALSE(kitzoo::util::Uuid::parse("550e8400e29b41d4a716446655440000"));
}

TEST(UuidTest, NilAndHashSupport) {
  EXPECT_TRUE(kitzoo::util::Uuid{}.is_nil());
  std::unordered_set<kitzoo::util::Uuid> values;
  values.insert(kitzoo::util::Uuid::random());
  EXPECT_EQ(values.size(), 1u);
}
