require "bundler"

module Bundler
  module Source
    class Git
      attr_reader :uri, :branch, :ref

      def initialize(options = {})
        @uri = (options[:uri] || options[:git] || options["uri"] || options["git"]).to_s
        @branch = options[:branch] || options["branch"] || options[:tag] || options["tag"]
        @ref = options[:ref] || options["ref"]
      end

      def to_s
        @uri
      end
    end
  end
end
