#pragma once
// PEOPLE-NAMES-NICKNAMES-2026-11-05-R1. Fresh construction only.
inline constexpr const char* CorePersonNicknamesSchema = R"PNN_SQL(
CREATE TABLE CorePersonNicknames (
 CorePersonKey TEXT NOT NULL REFERENCES CorePeople(CorePersonKey),
 NormalizedNickname TEXT NOT NULL CHECK(length(NormalizedNickname)>0),
 Nickname TEXT NOT NULL CHECK(length(trim(Nickname)) BETWEEN 1 AND 256),
 DisplayOrder INTEGER NOT NULL CHECK(DisplayOrder>=0),
 PRIMARY KEY(CorePersonKey,NormalizedNickname)
);
CREATE TABLE CorePersonNicknameSources (
 CorePersonKey TEXT NOT NULL,
 NormalizedNickname TEXT NOT NULL,
 SourceFileID INTEGER NOT NULL REFERENCES SourceFiles(SourceFileID),
 SourceLine INTEGER NOT NULL CHECK(SourceLine>0),
 PRIMARY KEY(CorePersonKey,NormalizedNickname,SourceFileID,SourceLine),
 FOREIGN KEY(CorePersonKey,NormalizedNickname)
  REFERENCES CorePersonNicknames(CorePersonKey,NormalizedNickname)
);
)PNN_SQL";
