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

# pragma once
# include <Siv3D/ChildProcess.hpp>
# include <sstream>

namespace s3d
{
	class ChildProcess::ChildProcessDetail
	{
	public:

		ChildProcessDetail();

		ChildProcessDetail(const FilePathView path, const StringView command, Pipe pipe);

		~ChildProcessDetail();

		bool isValid() const;

		bool isRunning();

		void wait();

		void terminate();

		Optional<int32> getExitCode();

		std::istream& istream();

		std::ostream& ostream();

	private:
		std::stringstream m_ops;
		std::stringstream m_ips;
	};
}
