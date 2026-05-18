require "bundler"

module Bundler
  module Source
    class Path
      attr_reader :path

      def initialize(options = {})
        raw = options[:path] || options["path"]
        @path = raw ? File.expand_path(raw.to_s) : nil
      end

      def to_s
        @path.to_s
      end
    end
  end
end
