require "bundler"

module Bundler
  module Source
    class Rubygems
      attr_reader :remotes

      def initialize(options = {})
        @remotes = Array(options[:remotes] || options[:remote]).map(&:to_s)
      end

      def to_s
        @remotes.join(", ")
      end
    end
  end
end
