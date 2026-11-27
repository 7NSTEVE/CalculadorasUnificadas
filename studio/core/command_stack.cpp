#include "command_stack.hpp"
#include "cudoc_storage.hpp"
#include "utf_conv.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace DocStudio {

namespace {

int NormalizeDegrees(int deg) noexcept {
    int r = deg % 360;
    if (r < 0) r += 360;
    return r;
}

bool AddToTarget(DocumentModel& doc, const ObjectTarget& target, AnnotationObject obj) {
    if (target.isCanvas) {
        doc.AddCanvasObject(std::move(obj));
        return true;
    }
    return doc.AddPageObject(target.pageIndex, std::move(obj));
}

bool RemoveFromTarget(DocumentModel& doc, const ObjectTarget& target, std::uint64_t id, AnnotationObject* out) {
    if (target.isCanvas) {
        return doc.RemoveCanvasObject(id, out);
    }
    return doc.RemovePageObject(target.pageIndex, id, out);
}

bool ReplaceInTarget(DocumentModel& doc, const ObjectTarget& target, const AnnotationObject& state) {
    if (target.isCanvas) {
        AnnotationObject* existing = doc.FindCanvasObjectMutable(state.id);
        if (!existing) return false;
        doc.AddCanvasObject(state);
        return true;
    }
    AnnotationObject* existing = doc.FindPageObjectMutable(target.pageIndex, state.id);
    if (!existing) return false;
    *existing = state;
    existing->RecomputeBounds();
    doc.BumpRevision();
    return true;
}

} // namespace

bool CommandStack::Execute(DocumentModel& doc, std::unique_ptr<ICommand> cmd) {
    if (!cmd) return false;
    if (!cmd->Execute(doc)) return false;

    if (cursor_ < history_.size()) {
        if (!cleanInvalidated_ && cleanCursor_ > cursor_) {
            cleanInvalidated_ = true;
        }
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(cursor_), history_.end());
    }

    history_.push_back(std::move(cmd));
    cursor_ = history_.size();

    if (maxDepth_ > 0 && history_.size() > maxDepth_) {
        const std::size_t dropCount = history_.size() - maxDepth_;
        history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(dropCount));
        cursor_ -= dropCount;
        undoBoundary_ = (undoBoundary_ > dropCount) ? (undoBoundary_ - dropCount) : 0;
        if (cleanCursor_ < dropCount) {
            cleanInvalidated_ = true;
            cleanCursor_ = 0;
        } else {
            cleanCursor_ -= dropCount;
        }
    }

    UpdateDirtyFlag(doc);
    return true;
}

bool CommandStack::Undo(DocumentModel& doc) {
    if (!CanUndo()) return false;
    auto& cmd = history_[cursor_ - 1];
    if (!cmd->Undo(doc)) return false;
    --cursor_;
    UpdateDirtyFlag(doc);
    return true;
}

bool CommandStack::Redo(DocumentModel& doc) {
    if (!CanRedo()) return false;
    auto& cmd = history_[cursor_];
    if (!cmd->Execute(doc)) return false;
    ++cursor_;
    UpdateDirtyFlag(doc);
    return true;
}

std::string CommandStack::UndoDescription() const {
    if (!CanUndo()) return {};
    return history_[cursor_ - 1]->Description();
}

std::string CommandStack::RedoDescription() const {
    if (!CanRedo()) return {};
    return history_[cursor_]->Description();
}

void CommandStack::MarkClean(DocumentModel& doc) noexcept {
    cleanCursor_ = cursor_;
    cleanInvalidated_ = false;
    doc.SetDirty(false);
}

void CommandStack::MarkCleanUnreachable(DocumentModel& doc) noexcept {
    // Mismo estado que cuando el punto «guardado» se pierde del historial: UpdateDirtyFlag deja el
    // documento modificado pase lo que pase con deshacer y rehacer, hasta que MarkClean lo limpie.
    cleanInvalidated_ = true;
    doc.SetDirty(true);
}

void CommandStack::Clear() noexcept {
    history_.clear();
    cursor_ = 0;
    cleanCursor_ = 0;
    undoBoundary_ = 0;
    cleanInvalidated_ = false;
}

void CommandStack::UpdateDirtyFlag(DocumentModel& doc) const noexcept {
    if (cleanInvalidated_) {
        doc.SetDirty(true);
    } else {
        doc.SetDirty(cursor_ != cleanCursor_);
    }
}

// AddObjectCommand
AddObjectCommand::AddObjectCommand(ObjectTarget target, AnnotationObject obj)
    : target_(target), obj_(std::move(obj)) {}

std::string AddObjectCommand::Description() const {
    return "Agregar objeto";
}

bool AddObjectCommand::Execute(DocumentModel& doc) {
    if (obj_.id == 0) obj_.id = doc.AllocateObjectId();
    if (obj_.zOrder == 0) obj_.zOrder = doc.AllocateZOrder();
    obj_.RecomputeBounds();
    return AddToTarget(doc, target_, obj_);
}

bool AddObjectCommand::Undo(DocumentModel& doc) {
    return RemoveFromTarget(doc, target_, obj_.id, nullptr);
}

// BatchAddObjectsCommand
BatchAddObjectsCommand::BatchAddObjectsCommand(ObjectTarget target, std::vector<AnnotationObject> objs, std::string desc)
    : target_(target), objs_(std::move(objs)), desc_(std::move(desc)) {}

std::string BatchAddObjectsCommand::Description() const {
    return desc_;
}

bool BatchAddObjectsCommand::Execute(DocumentModel& doc) {
    if (objs_.empty()) return false;
    bool any = false;
    for (auto& o : objs_) {
        if (o.id == 0) o.id = doc.AllocateObjectId();
        if (o.zOrder == 0) o.zOrder = doc.AllocateZOrder();
        o.RecomputeBounds();
        any = AddToTarget(doc, target_, o) || any;
    }
    return any;
}

bool BatchAddObjectsCommand::Undo(DocumentModel& doc) {
    bool any = false;
    for (auto it = objs_.rbegin(); it != objs_.rend(); ++it) {
        any = RemoveFromTarget(doc, target_, it->id, nullptr) || any;
    }
    return any;
}

// RemoveObjectCommand
RemoveObjectCommand::RemoveObjectCommand(ObjectTarget target, std::uint64_t objectId)
    : target_(target), objectId_(objectId) {}

std::string RemoveObjectCommand::Description() const {
    return "Eliminar objeto";
}

bool RemoveObjectCommand::Execute(DocumentModel& doc) {
    AnnotationObject removed;
    if (!RemoveFromTarget(doc, target_, objectId_, &removed)) return false;
    captured_ = std::move(removed);
    return true;
}

bool RemoveObjectCommand::Undo(DocumentModel& doc) {
    if (!captured_.has_value()) return false;
    return AddToTarget(doc, target_, *captured_);
}

// BatchEraseObjectsCommand
BatchEraseObjectsCommand::BatchEraseObjectsCommand(ObjectTarget target, std::vector<std::uint64_t> objectIds)
    : target_(target), objectIds_(std::move(objectIds)) {}

std::string BatchEraseObjectsCommand::Description() const {
    return "Borrador de trazos";
}

bool BatchEraseObjectsCommand::Execute(DocumentModel& doc) {
    captured_.clear();
    std::unordered_set<std::uint64_t> seen;
    for (std::uint64_t id : objectIds_) {
        if (!seen.insert(id).second) continue;
        AnnotationObject removed;
        if (RemoveFromTarget(doc, target_, id, &removed)) {
            captured_.push_back(std::move(removed));
        }
    }
    return !captured_.empty();
}

bool BatchEraseObjectsCommand::Undo(DocumentModel& doc) {
    if (captured_.empty()) return false;
    bool allOk = true;
    for (const auto& obj : captured_) {
        allOk = AddToTarget(doc, target_, obj) && allOk;
    }
    return allOk;
}

// ModifyObjectCommand
ModifyObjectCommand::ModifyObjectCommand(ObjectTarget target, AnnotationObject beforeState, AnnotationObject afterState)
    : target_(target), before_(std::move(beforeState)), after_(std::move(afterState)) {}

std::string ModifyObjectCommand::Description() const {
    return "Modificar objeto";
}

bool ModifyObjectCommand::Execute(DocumentModel& doc) {
    return ReplaceInTarget(doc, target_, after_);
}

bool ModifyObjectCommand::Undo(DocumentModel& doc) {
    return ReplaceInTarget(doc, target_, before_);
}

// BatchModifyObjectsCommand
BatchModifyObjectsCommand::BatchModifyObjectsCommand(
    ObjectTarget target,
    std::vector<AnnotationObject> beforeStates,
    std::vector<AnnotationObject> afterStates,
    std::string desc)
    : target_(target), before_(std::move(beforeStates)), after_(std::move(afterStates)), desc_(std::move(desc)) {}

std::string BatchModifyObjectsCommand::Description() const {
    return desc_.empty() ? "Transformar objetos" : desc_;
}

bool BatchModifyObjectsCommand::Execute(DocumentModel& doc) {
    if (after_.empty()) return false;
    bool anyOk = false;
    for (const auto& st : after_) {
        anyOk = ReplaceInTarget(doc, target_, st) || anyOk;
    }
    return anyOk;
}

bool BatchModifyObjectsCommand::Undo(DocumentModel& doc) {
    if (before_.empty()) return false;
    bool anyOk = false;
    for (const auto& st : before_) {
        anyOk = ReplaceInTarget(doc, target_, st) || anyOk;
    }
    return anyOk;
}

// ReplaceStrokeWithSplitsCommand
ReplaceStrokeWithSplitsCommand::ReplaceStrokeWithSplitsCommand(
    ObjectTarget target,
    AnnotationObject originalStroke,
    std::vector<AnnotationObject> replacementStrokes)
    : target_(target), original_(std::move(originalStroke)), replacements_(std::move(replacementStrokes)) {}

std::string ReplaceStrokeWithSplitsCommand::Description() const {
    return "Borrado parcial de trazo";
}

bool ReplaceStrokeWithSplitsCommand::Execute(DocumentModel& doc) {
    if (!RemoveFromTarget(doc, target_, original_.id, nullptr)) return false;
    for (auto& rep : replacements_) {
        if (rep.id == 0 || rep.id == original_.id) rep.id = doc.AllocateObjectId();
        if (rep.zOrder == 0) rep.zOrder = doc.AllocateZOrder();
        rep.RecomputeBounds();
        AddToTarget(doc, target_, rep);
    }
    return true;
}

bool ReplaceStrokeWithSplitsCommand::Undo(DocumentModel& doc) {
    for (const auto& rep : replacements_) {
        RemoveFromTarget(doc, target_, rep.id, nullptr);
    }
    return AddToTarget(doc, target_, original_);
}

// SnapshotDeltaCommand
SnapshotDeltaCommand::SnapshotDeltaCommand(
    std::vector<std::uint8_t> beforeCudocBytes,
    std::vector<std::uint8_t> afterCudocBytes,
    std::string desc)
    : beforeBytes_(std::move(beforeCudocBytes)), afterBytes_(std::move(afterCudocBytes)), desc_(std::move(desc)) {}

bool SnapshotDeltaCommand::Execute(DocumentModel& doc) {
    if (afterBytes_.empty()) return false;
    return CudocStorage::DeserializeFromBytes(afterBytes_.data(), afterBytes_.size(), doc, nullptr);
}

bool SnapshotDeltaCommand::Undo(DocumentModel& doc) {
    if (beforeBytes_.empty()) return false;
    return CudocStorage::DeserializeFromBytes(beforeBytes_.data(), beforeBytes_.size(), doc, nullptr);
}

namespace {
std::string Base64EncodeHist(const std::vector<std::uint8_t>& in) {
    static constexpr char kT[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < in.size(); i += 3) {
        const std::uint32_t b0 = in[i];
        const std::uint32_t b1 = (i + 1 < in.size()) ? in[i + 1] : 0;
        const std::uint32_t b2 = (i + 2 < in.size()) ? in[i + 2] : 0;
        const std::uint32_t t = (b0 << 16) | (b1 << 8) | b2;
        out.push_back(kT[(t >> 18) & 0x3Fu]);
        out.push_back(kT[(t >> 12) & 0x3Fu]);
        out.push_back((i + 1 < in.size()) ? kT[(t >> 6) & 0x3Fu] : '=');
        out.push_back((i + 2 < in.size()) ? kT[t & 0x3Fu] : '=');
    }
    return out;
}

std::vector<std::uint8_t> Base64DecodeHist(const std::string& in) {
    auto val = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    out.reserve((in.size() * 3) / 4);
    std::uint32_t acc = 0;
    int bits = 0;
    for (unsigned char c : in) {
        if (c == '=') break;
        const int v = val(c);
        if (v < 0) continue;
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFFu));
        }
    }
    return out;
}
} // namespace

void CommandStack::RecordPersistentCheckpoint(
    const std::vector<std::uint8_t>& beforeBytes,
    const std::vector<std::uint8_t>& afterBytes,
    std::string desc)
{
    if (beforeBytes.empty() || afterBytes.empty()) return;
    if (cursor_ < history_.size()) {
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(cursor_), history_.end());
    }
    history_.push_back(std::make_unique<SnapshotDeltaCommand>(beforeBytes, afterBytes, std::move(desc)));
    cursor_ = history_.size();
}

bool CommandStack::SaveSessionHistoryToFile(const std::wstring& historyPath, const DocumentModel& currentDoc) const {
    try {
        nlohmann::json root;
        root["version"] = 1;
        root["cursor"] = 0;
        nlohmann::json entries = nlohmann::json::array();

        // Synthesize a contiguous bounded undo chain (up to 16 steps) by replaying backward
        // from currentDoc across ALL command types (both loaded SnapshotDeltaCommands AND new live edits),
        // and also preserve any forward redo SnapshotDeltaCommands beyond cursor_.
        if (cursor_ > undoBoundary_) {
            const std::size_t maxPersist = 16;
            const std::size_t avail = cursor_ - undoBoundary_;
            const std::size_t take = (std::min)(avail, maxPersist);
            std::vector<std::vector<std::uint8_t>> states(take + 1);
            DocumentModel scratch;
            if (CudocStorage::SerializeToBytes(currentDoc, states[take], nullptr) &&
                CudocStorage::DeserializeFromBytes(states[take].data(), states[take].size(), scratch, nullptr)) {
                std::size_t built = 0;
                std::vector<std::string> descs(take);
                for (std::size_t k = 0; k < take; ++k) {
                    const std::size_t cmdIdx = cursor_ - 1 - k;
                    descs[take - 1 - k] = history_[cmdIdx]->Description();
                    if (!history_[cmdIdx]->Undo(scratch) ||
                        !CudocStorage::SerializeToBytes(scratch, states[take - 1 - k], nullptr)) {
                        break;
                    }
                    ++built;
                }
                const std::size_t startSlot = take - built;
                for (std::size_t i = startSlot; i < take; ++i) {
                    entries.push_back({
                        {"desc", descs[i]},
                        {"before", Base64EncodeHist(states[i])},
                        {"after", Base64EncodeHist(states[i + 1])}
                    });
                }
                root["cursor"] = entries.size();
            }
        }
        // Preserve redo trail beyond cursor_ if already stored as SnapshotDeltaCommand
        for (std::size_t i = cursor_; i < history_.size() && entries.size() < 24; ++i) {
            if (const auto* sc = dynamic_cast<const SnapshotDeltaCommand*>(history_[i].get())) {
                entries.push_back({
                    {"desc", sc->Description()},
                    {"before", Base64EncodeHist(sc->BeforeBytes())},
                    {"after", Base64EncodeHist(sc->AfterBytes())}
                });
            }
        }
        root["entries"] = std::move(entries);

        const std::filesystem::path p(WideToUtf8(historyPath));
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs) return false;
        const std::string dump = root.dump();
        ofs.write(dump.data(), static_cast<std::streamsize>(dump.size()));
        return ofs.good();
    } catch (...) {
        return false;
    }
}

bool CommandStack::LoadSessionHistoryFromFile(const std::wstring& historyPath, DocumentModel& doc) {
    try {
        const std::filesystem::path p(WideToUtf8(historyPath));
        std::ifstream ifs(p, std::ios::binary);
        if (!ifs) return false;
        const nlohmann::json root = nlohmann::json::parse(ifs);
        if (!root.is_object() || !root.contains("entries") || !root["entries"].is_array()) return false;

        Clear();
        for (const auto& ej : root["entries"]) {
            if (!ej.is_object()) continue;
            auto b = Base64DecodeHist(ej.value("before", ""));
            auto a = Base64DecodeHist(ej.value("after", ""));
            if (b.empty() || a.empty()) continue;
            history_.push_back(std::make_unique<SnapshotDeltaCommand>(
                std::move(b), std::move(a), ej.value("desc", "Historial persistente")));
        }
        cursor_ = (std::min)(root.value("cursor", history_.size()), history_.size());
        cleanCursor_ = cursor_;
        undoBoundary_ = 0;
        cleanInvalidated_ = false;
        doc.SetDirty(false);
        return !history_.empty();
    } catch (...) {
        return false;
    }
}

// SetBackgroundCommand
SetBackgroundCommand::SetBackgroundCommand(ObjectTarget target, PaperPattern newBg)
    : target_(target), newBg_(newBg) {}

std::string SetBackgroundCommand::Description() const {
    return "Cambiar fondo de papel";
}

bool SetBackgroundCommand::Execute(DocumentModel& doc) {
    if (target_.isCanvas) {
        if (!capturedOld_) {
            oldBg_ = doc.CanvasBackground();
            capturedOld_ = true;
        }
        doc.SetCanvasBackground(newBg_);
        return true;
    }
    auto& pages = doc.MutablePages();
    if (target_.pageIndex >= pages.size()) return false;
    if (!capturedOld_) {
        oldBg_ = pages[target_.pageIndex].background;
        capturedOld_ = true;
    }
    pages[target_.pageIndex].background = newBg_;
    doc.BumpRevision();
    return true;
}

bool SetBackgroundCommand::Undo(DocumentModel& doc) {
    if (!capturedOld_) return false;
    if (target_.isCanvas) {
        doc.SetCanvasBackground(oldBg_);
        return true;
    }
    auto& pages = doc.MutablePages();
    if (target_.pageIndex >= pages.size()) return false;
    pages[target_.pageIndex].background = oldBg_;
    doc.BumpRevision();
    return true;
}

// InsertPageCommand
InsertPageCommand::InsertPageCommand(std::size_t insertAtIndex, PageModel page)
    : insertAtIndex_(insertAtIndex), page_(std::move(page)) {}

std::string InsertPageCommand::Description() const {
    return "Insertar p\xC3\xA1gina";
}

bool InsertPageCommand::Execute(DocumentModel& doc) {
    auto& pages = doc.MutablePages();
    if (page_.pageId == 0) page_.pageId = doc.AllocatePageId();
    const std::size_t pos = (std::min)(insertAtIndex_, pages.size());
    insertAtIndex_ = pos;
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(pos), page_);
    doc.RebuildLayout();
    return true;
}

bool InsertPageCommand::Undo(DocumentModel& doc) {
    auto& pages = doc.MutablePages();
    if (pages.size() <= 1 || insertAtIndex_ >= pages.size()) return false;
    pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(insertAtIndex_));
    doc.RebuildLayout();
    return true;
}

// DeletePageCommand
DeletePageCommand::DeletePageCommand(std::size_t pageIndex)
    : pageIndex_(pageIndex) {}

std::string DeletePageCommand::Description() const {
    return "Eliminar p\xC3\xA1gina";
}

bool DeletePageCommand::Execute(DocumentModel& doc) {
    auto& pages = doc.MutablePages();
    if (pages.size() <= 1 || pageIndex_ >= pages.size()) return false;
    capturedPage_ = pages[pageIndex_];
    pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(pageIndex_));
    doc.RebuildLayout();
    return true;
}

bool DeletePageCommand::Undo(DocumentModel& doc) {
    if (!capturedPage_.has_value()) return false;
    auto& pages = doc.MutablePages();
    const std::size_t pos = (std::min)(pageIndex_, pages.size());
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(pos), *capturedPage_);
    doc.RebuildLayout();
    return true;
}

// ReorderPageCommand
ReorderPageCommand::ReorderPageCommand(std::size_t fromIndex, std::size_t toIndex)
    : fromIndex_(fromIndex), toIndex_(toIndex) {}

std::string ReorderPageCommand::Description() const {
    return "Reordenar p\xC3\xA1gina";
}

bool ReorderPageCommand::Execute(DocumentModel& doc) {
    auto& pages = doc.MutablePages();
    if (fromIndex_ >= pages.size() || toIndex_ >= pages.size() || fromIndex_ == toIndex_) return false;
    PageModel moved = std::move(pages[fromIndex_]);
    pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(fromIndex_));
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(toIndex_), std::move(moved));
    doc.RebuildLayout();
    return true;
}

bool ReorderPageCommand::Undo(DocumentModel& doc) {
    auto& pages = doc.MutablePages();
    if (toIndex_ >= pages.size() || fromIndex_ >= pages.size() || fromIndex_ == toIndex_) return false;
    PageModel moved = std::move(pages[toIndex_]);
    pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(toIndex_));
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(fromIndex_), std::move(moved));
    doc.RebuildLayout();
    return true;
}

// RotatePageCommand
RotatePageCommand::RotatePageCommand(std::size_t pageIndex, int deltaDegrees)
    : pageIndex_(pageIndex), deltaDeg_(deltaDegrees) {}

std::string RotatePageCommand::Description() const {
    return "Rotar p\xC3\xA1gina";
}

bool RotatePageCommand::Execute(DocumentModel& doc) {
    auto& pages = doc.MutablePages();
    if (pageIndex_ >= pages.size()) return false;
    if (!capturedOld_) {
        oldDeg_ = pages[pageIndex_].rotationDeg;
        capturedOld_ = true;
    }
    pages[pageIndex_].rotationDeg = NormalizeDegrees(oldDeg_ + deltaDeg_);
    doc.RebuildLayout();
    return true;
}

bool RotatePageCommand::Undo(DocumentModel& doc) {
    if (!capturedOld_) return false;
    auto& pages = doc.MutablePages();
    if (pageIndex_ >= pages.size()) return false;
    pages[pageIndex_].rotationDeg = oldDeg_;
    doc.RebuildLayout();
    return true;
}

// CropPageCommand
CropPageCommand::CropPageCommand(std::size_t pageIndex, NormalizedCropRect newCrop)
    : pageIndex_(pageIndex), newCrop_(newCrop) {}

std::string CropPageCommand::Description() const {
    return "Recortar p\xC3\xA1gina";
}

bool CropPageCommand::Execute(DocumentModel& doc) {
    auto& pages = doc.MutablePages();
    if (pageIndex_ >= pages.size()) return false;
    if (!capturedOld_) {
        oldCrop_ = pages[pageIndex_].crop;
        capturedOld_ = true;
    }
    NormalizedCropRect c = newCrop_;
    c.Clamp();
    pages[pageIndex_].crop = c;
    doc.RebuildLayout();
    return true;
}

bool CropPageCommand::Undo(DocumentModel& doc) {
    if (!capturedOld_) return false;
    auto& pages = doc.MutablePages();
    if (pageIndex_ >= pages.size()) return false;
    pages[pageIndex_].crop = oldCrop_;
    doc.RebuildLayout();
    return true;
}

} // namespace DocStudio
