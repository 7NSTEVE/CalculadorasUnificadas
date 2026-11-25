#pragma once

#include "doc_types.hpp"
#include "document_model.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace DocStudio {

struct ObjectTarget {
    bool isCanvas = false;
    std::size_t pageIndex = 0; // used when !isCanvas
};

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string Description() const = 0;
    virtual bool Execute(DocumentModel& doc) = 0;
    virtual bool Undo(DocumentModel& doc) = 0;
};

class CommandStack {
public:
    explicit CommandStack(std::size_t maxDepth = 256)
        : maxDepth_(maxDepth) {}

    bool Execute(DocumentModel& doc, std::unique_ptr<ICommand> cmd);
    bool Undo(DocumentModel& doc);
    bool Redo(DocumentModel& doc);

    bool CanUndo() const noexcept { return cursor_ > undoBoundary_; }
    bool CanRedo() const noexcept { return cursor_ < history_.size(); }
    std::size_t UndoCount() const noexcept { return cursor_ > undoBoundary_ ? (cursor_ - undoBoundary_) : 0; }
    std::size_t RedoCount() const noexcept { return history_.size() - cursor_; }

    // Safety boundary preventing Undo from erasing initial loaded/recovered document state
    void SetUndoBoundary() noexcept { undoBoundary_ = cursor_; }
    void ClearUndoBoundary() noexcept { undoBoundary_ = 0; }
    std::size_t UndoBoundary() const noexcept { return undoBoundary_; }

    // Pause background autosave while crash recovery / conflict merge is active
    void SetAutoSavePaused(bool paused) noexcept { autoSavePaused_ = paused; }
    bool IsAutoSavePaused() const noexcept { return autoSavePaused_; }

    std::string UndoDescription() const;
    std::string RedoDescription() const;

    void MarkClean(DocumentModel& doc) noexcept;
    void Clear() noexcept;

    // Persistent cross-session Undo/Redo snapshot history (F-004)
    bool SaveSessionHistoryToFile(const std::wstring& historyPath, const DocumentModel& currentDoc) const;
    bool LoadSessionHistoryFromFile(const std::wstring& historyPath, DocumentModel& doc);
    void RecordPersistentCheckpoint(const std::vector<std::uint8_t>& beforeBytes, const std::vector<std::uint8_t>& afterBytes, std::string desc);

private:
    std::vector<std::unique_ptr<ICommand>> history_;
    std::size_t cursor_ = 0;
    std::size_t cleanCursor_ = 0;
    std::size_t undoBoundary_ = 0;
    bool cleanInvalidated_ = false;
    bool autoSavePaused_ = false;
    std::size_t maxDepth_ = 256;

    void UpdateDirtyFlag(DocumentModel& doc) const noexcept;
};

// Concrete reversible commands
class AddObjectCommand final : public ICommand {
public:
    AddObjectCommand(ObjectTarget target, AnnotationObject obj);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;
    std::uint64_t ObjectId() const noexcept { return obj_.id; }

private:
    ObjectTarget target_;
    AnnotationObject obj_;
};

class BatchAddObjectsCommand final : public ICommand {
public:
    BatchAddObjectsCommand(ObjectTarget target, std::vector<AnnotationObject> objs, std::string desc = "Agregar grupo de objetos");
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;
    const std::vector<AnnotationObject>& Objects() const noexcept { return objs_; }

private:
    ObjectTarget target_;
    std::vector<AnnotationObject> objs_;
    std::string desc_;
};

class RemoveObjectCommand final : public ICommand {
public:
    RemoveObjectCommand(ObjectTarget target, std::uint64_t objectId);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    ObjectTarget target_;
    std::uint64_t objectId_ = 0;
    std::optional<AnnotationObject> captured_;
};

class BatchEraseObjectsCommand final : public ICommand {
public:
    BatchEraseObjectsCommand(ObjectTarget target, std::vector<std::uint64_t> objectIds);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    ObjectTarget target_;
    std::vector<std::uint64_t> objectIds_;
    std::vector<AnnotationObject> captured_;
};

class ModifyObjectCommand final : public ICommand {
public:
    ModifyObjectCommand(ObjectTarget target, AnnotationObject beforeState, AnnotationObject afterState);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    ObjectTarget target_;
    AnnotationObject before_;
    AnnotationObject after_;
};

class BatchModifyObjectsCommand final : public ICommand {
public:
    BatchModifyObjectsCommand(
        ObjectTarget target,
        std::vector<AnnotationObject> beforeStates,
        std::vector<AnnotationObject> afterStates,
        std::string desc = "Transformar objetos");
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    ObjectTarget target_;
    std::vector<AnnotationObject> before_;
    std::vector<AnnotationObject> after_;
    std::string desc_;
};

class ReplaceStrokeWithSplitsCommand final : public ICommand {
public:
    ReplaceStrokeWithSplitsCommand(
        ObjectTarget target,
        AnnotationObject originalStroke,
        std::vector<AnnotationObject> replacementStrokes);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    ObjectTarget target_;
    AnnotationObject original_;
    std::vector<AnnotationObject> replacements_;
};

class SnapshotDeltaCommand final : public ICommand {
public:
    SnapshotDeltaCommand(
        std::vector<std::uint8_t> beforeCudocBytes,
        std::vector<std::uint8_t> afterCudocBytes,
        std::string desc = "Cambio de sesi\xC3\xB3n");
    std::string Description() const override { return desc_; }
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;
    const std::vector<std::uint8_t>& BeforeBytes() const noexcept { return beforeBytes_; }
    const std::vector<std::uint8_t>& AfterBytes() const noexcept { return afterBytes_; }

private:
    std::vector<std::uint8_t> beforeBytes_;
    std::vector<std::uint8_t> afterBytes_;
    std::string desc_;
};

class SetBackgroundCommand final : public ICommand {
public:
    SetBackgroundCommand(ObjectTarget target, PaperPattern newBg);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    ObjectTarget target_;
    PaperPattern newBg_ = PaperPattern::Plain;
    PaperPattern oldBg_ = PaperPattern::Plain;
    bool capturedOld_ = false;
};

class InsertPageCommand final : public ICommand {
public:
    InsertPageCommand(std::size_t insertAtIndex, PageModel page);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    std::size_t insertAtIndex_ = 0;
    PageModel page_;
};

class DeletePageCommand final : public ICommand {
public:
    explicit DeletePageCommand(std::size_t pageIndex);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    std::size_t pageIndex_ = 0;
    std::optional<PageModel> capturedPage_;
};

class ReorderPageCommand final : public ICommand {
public:
    ReorderPageCommand(std::size_t fromIndex, std::size_t toIndex);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    std::size_t fromIndex_ = 0;
    std::size_t toIndex_ = 0;
};

class RotatePageCommand final : public ICommand {
public:
    RotatePageCommand(std::size_t pageIndex, int deltaDegrees);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    std::size_t pageIndex_ = 0;
    int deltaDeg_ = 90;
    int oldDeg_ = 0;
    bool capturedOld_ = false;
};

class CropPageCommand final : public ICommand {
public:
    CropPageCommand(std::size_t pageIndex, NormalizedCropRect newCrop);
    std::string Description() const override;
    bool Execute(DocumentModel& doc) override;
    bool Undo(DocumentModel& doc) override;

private:
    std::size_t pageIndex_ = 0;
    NormalizedCropRect newCrop_{};
    NormalizedCropRect oldCrop_{};
    bool capturedOld_ = false;
};

} // namespace DocStudio
