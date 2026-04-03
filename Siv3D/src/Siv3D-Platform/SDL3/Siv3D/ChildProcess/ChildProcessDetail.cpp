//-----------------------------------------------
//
//	This file is part of the Siv3D Engine.
//
//	Copyright (c) 2008-2025 Ryo Suzuki
//	Copyright (c) 2016-2025 OpenSiv3D Project
//
//	Licensed under the MIT License.
//
//-----------------------------------------------

# include <Siv3D/FileSystem.hpp>
# include "ChildProcessDetail.hpp"
\
namespace s3d
{
	ChildProcess::ChildProcessDetail::ChildProcessDetail() {}

	ChildProcess::ChildProcessDetail::ChildProcessDetail(const FilePathView path, const StringView command, const Pipe pipe)
	{
		const std::wstring pathW = path.toWstr();
		const std::wstring commandW = command.toWstr();
		const std::wstring startDir = FileSystem::ParentPath(path).toWstr();

		switch (pipe)
		{
		case Pipe::None_:
			break;
		case Pipe::StdIn:
			break;
		case Pipe::StdOut:
			break;
		case Pipe::StdInOut:
			break;
		}
	}

	ChildProcess::ChildProcessDetail::~ChildProcessDetail()
	{
	}

	bool ChildProcess::ChildProcessDetail::isValid() const
	{
		return false;
	}

	bool ChildProcess::ChildProcessDetail::isRunning()
	{
		return false;
	}

	void ChildProcess::ChildProcessDetail::wait()
	{		
	}

	void ChildProcess::ChildProcessDetail::terminate()
	{
	}

	Optional<int32> ChildProcess::ChildProcessDetail::getExitCode()
	{
		return none;
	}

	std::istream& ChildProcess::ChildProcessDetail::istream()
	{
		return m_ips;
	}

	std::ostream& ChildProcess::ChildProcessDetail::ostream()
	{
		return m_ops;
	}
}
