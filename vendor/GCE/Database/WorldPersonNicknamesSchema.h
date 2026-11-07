#pragma once
// PEOPLE-NAMES-NICKNAMES-2026-11-05-R1. Fresh construction only.
inline constexpr const char* WorldPersonNicknamesSchema = R"PNN_SQL(
CREATE UNIQUE INDEX PNN_SourceAliasPerson ON WorldPersonSourceAliases
 (WorldID,WorldCoreSnapshotID,CorePersonKey,PersonID);
CREATE TABLE WorldPersonNicknames (
 WorldID INTEGER NOT NULL,
 PersonID INTEGER NOT NULL,
 NormalizedNickname TEXT NOT NULL CHECK(length(NormalizedNickname)>0),
 Nickname TEXT NOT NULL CHECK(length(trim(Nickname)) BETWEEN 1 AND 256),
 DisplayOrder INTEGER NOT NULL CHECK(DisplayOrder>=0),
 OriginKind TEXT NOT NULL CHECK(OriginKind IN ('CORE','WORLD')),
 ChangedRevision INTEGER NOT NULL CHECK(ChangedRevision>=0),
 PRIMARY KEY(WorldID,PersonID,NormalizedNickname),
 FOREIGN KEY(WorldID,PersonID) REFERENCES People(WorldID,PersonID)
);
CREATE TABLE WorldPersonNicknameSources (
 WorldID INTEGER NOT NULL,
 PersonID INTEGER NOT NULL,
 NormalizedNickname TEXT NOT NULL,
 WorldCoreSnapshotID INTEGER NOT NULL,
 CorePersonKey TEXT NOT NULL,
 PRIMARY KEY(WorldID,PersonID,NormalizedNickname,WorldCoreSnapshotID,CorePersonKey),
 FOREIGN KEY(WorldID,PersonID,NormalizedNickname)
  REFERENCES WorldPersonNicknames(WorldID,PersonID,NormalizedNickname),
 FOREIGN KEY(WorldID,WorldCoreSnapshotID,CorePersonKey,PersonID)
  REFERENCES WorldPersonSourceAliases(WorldID,WorldCoreSnapshotID,CorePersonKey,PersonID)
);
)PNN_SQL";
