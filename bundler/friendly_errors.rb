require "bundler"

module Bundler
  module FriendlyErrors
    class << self
      def exit_status(error)
        return 0 unless error
        return 7 if error.is_a?(Bundler::GemNotFound)
        return 4 if error.is_a?(Bundler::GemfileNotFound) || error.is_a?(Bundler::LockfileError)
        return 15 if error.is_a?(Bundler::PermissionError)
        return 1 if error.is_a?(Bundler::BundlerError)
        1
      end

      def friendly_error_message(error)
        error.message.to_s
      end

      def with_friendly_errors
        yield if block_given?
      end
    end
  end
end
