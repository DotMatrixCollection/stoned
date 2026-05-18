require "bundler"

module Bundler
  module Source
    class Rubygems
      attr_reader :remotes, :options

      def initialize(options = {})
        @options = options
        @remotes = Array(options[:remotes] || options[:remote]).map(&:to_s)
      end

      def to_s
        @remotes.join(", ")
      end

      def remote!
        @remotes[0]
      end
    end
  end
end
